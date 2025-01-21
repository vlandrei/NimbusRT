#include "PointCloudEnvironment.hpp"
#include "Nimbus/Utils.hpp"
#include "KernelData.hpp"
#include "Logger.hpp"
#include <unordered_map>

namespace Nimbus
{
	PointCloudEnvironment::PointCloudEnvironment()
        : m_Aabb()
        , m_VoxelWorldInfo()
        , m_IeCount(0u)
        , m_PointCount(0u)
	{
	}

	bool PointCloudEnvironment::Init(const PointData* points, size_t numPoints, float voxelSize, float aabbBias)
	{
		if (numPoints <= 1u)
			return false;

        std::vector<PointNode> pointNodes = LoadPoints(points, numPoints);
        
        if (!ComputeVoxelWorld(voxelSize))
        {
            LOG("Failed to compute voxel world. Voxel size is 0 or voxel world dimensions are 0.");
            return false;
        }
    
        std::vector<glm::uvec2> voxelNodeIndices = LinkPointNodes(pointNodes);

        if (!GenerateRayTracingData(pointNodes, voxelNodeIndices, aabbBias))
        {
            LOG("Failed to generate ray tracing data.");
            return false;
        }
        return true;
	}

    EnvironmentData PointCloudEnvironment::GetGpuEnvironmentData() const
    {
        EnvironmentData result{};
        result.asHandle = m_AccelerationStructure.GetRawHandle();
        result.rtPoints = m_RtPointBuffer.DevicePointerCast<glm::vec3>();
        result.vwInfo = m_VoxelWorldInfo;
        result.pc.primitiveInfos = m_PrimitiveInfoBuffer.DevicePointerCast<IEPrimitiveInfo>();
        result.pc.primitivePoints = m_PrimitivePointBuffer.DevicePointerCast<PrimitivePoint>();
        result.pc.primitives = m_PrimitiveBuffer.DevicePointerCast<OptixAabb>();
        return result;
    }

    void PointCloudEnvironment::ComputeVisibility(const DeviceBuffer& params, const glm::uvec3& dims) const
    {
        KernelData::Get().GetStVisPipeline().LaunchAndSynchronize(params, dims);
    }

    void PointCloudEnvironment::DetermineLosPaths(const DeviceBuffer& params, const glm::uvec3& dims) const
    {
        KernelData::Get().GetStTransmitLOSPipeline().LaunchAndSynchronize(params, dims);
    }

    void PointCloudEnvironment::Transmit(const DeviceBuffer& params, const glm::uvec3& dims) const
    {
        KernelData::Get().GetStTransmitPipeline().LaunchAndSynchronize(params, dims);
    }

    void PointCloudEnvironment::Propagate(const DeviceBuffer& params, const glm::uvec3& dims) const
    {
        KernelData::Get().GetStPropagatePipeline().LaunchAndSynchronize(params, dims);
    }

    void PointCloudEnvironment::RefineSpecular(const DeviceBuffer& params, const glm::uvec3& dims) const
    {
        KernelData::Get().GetStRefineSpecularPipeline().LaunchAndSynchronize(params, dims);
    }

    void PointCloudEnvironment::RefineScatterer(const DeviceBuffer& params, const glm::uvec3& dims) const
    {
        KernelData::Get().GetStRefineScattererPipeline().LaunchAndSynchronize(params, dims);
    }

    void PointCloudEnvironment::RefineDiffraction(const DeviceBuffer& params, const glm::uvec3& dims) const
    {
        KernelData::Get().GetStRefineDiffractionPipeline().LaunchAndSynchronize(params, dims);
    }

	std::vector<PointNode> PointCloudEnvironment::LoadPoints(const PointData* points, size_t numPoints)
	{
        std::vector<PointNode> pointNodes;
        pointNodes.reserve(numPoints);
        m_Aabb.min = points->position;
        m_Aabb.max = points->position;

        for (size_t i = 0; i < numPoints; ++i)
        {
            const PointData& point = points[i];
            PointNode node{};
            node.position = point.position;
            node.normal = point.normal;
            node.label = point.label;
            node.materialID = point.material;
            node.ieNext = Constants::InvalidPointIndex;
            pointNodes.push_back(node);

            m_Aabb.min = glm::min(m_Aabb.min, point.position);
            m_Aabb.max = glm::max(m_Aabb.max, point.position);
        }
        constexpr float bias = 0.01f;
        m_Aabb.min -= bias;
        m_Aabb.max += bias;
        return pointNodes;
	}

    bool PointCloudEnvironment::ComputeVoxelWorld(float voxelSize)
    {
        glm::uvec3 voxelDimensions = glm::uvec3(glm::ceil((m_Aabb.max - m_Aabb.min) / voxelSize));
        m_VoxelWorldInfo = VoxelWorldInfo(m_Aabb.min, voxelSize, voxelDimensions);
        return voxelDimensions.x > 0 && voxelDimensions.y > 0 && voxelDimensions.z > 0 && voxelSize > 0.0f;
    }

    std::vector<glm::uvec2> PointCloudEnvironment::LinkPointNodes(std::vector<PointNode>& pointNodes)
    {
        std::vector<glm::uvec2> voxelNodeIndices;
        voxelNodeIndices.reserve(m_VoxelWorldInfo.count);
        std::unordered_map<uint64_t, uint32_t> primitiveHashMap;

        for (uint32_t pointIndex = 0; pointIndex < pointNodes.size(); ++pointIndex)
        {
            PointNode& pointNode = pointNodes[pointIndex];
            uint64_t voxelID = Utils::WorldToVoxelID(pointNode.position, m_VoxelWorldInfo);
            uint64_t id = (voxelID << 32u) | pointNode.label;
            auto it = primitiveHashMap.find(id);
            if (it == primitiveHashMap.end())
            {
                it = primitiveHashMap.try_emplace(id, m_IeCount++).first;
                voxelNodeIndices.emplace_back(Nimbus::Constants::InvalidPointIndex, 0);
            }
            glm::uvec2& parent = voxelNodeIndices.at(it->second);
            pointNode.ieNext = parent.x;
            parent.x = pointIndex;
            ++parent.y;
        }
        return voxelNodeIndices;
    }

    bool PointCloudEnvironment::GenerateRayTracingData(const std::vector<PointNode>& pointNodes, const std::vector<glm::uvec2>& voxelNodeIndices, float aabbBias)
    {
        m_PrimitiveBuffer = DeviceBuffer(m_IeCount * sizeof(OptixAabb));
        m_RtPointBuffer = DeviceBuffer(m_IeCount * sizeof(glm::vec3));
        m_PrimitiveInfoBuffer = DeviceBuffer(m_IeCount * sizeof(IEPrimitiveInfo));
        m_PrimitivePointBuffer = DeviceBuffer(pointNodes.size() * sizeof(PrimitivePoint));
        
        DeviceBuffer pointNodeBuffer = DeviceBuffer::Create(pointNodes);
        DeviceBuffer voxelPointNodeIndicesBuffer = DeviceBuffer::Create(voxelNodeIndices);
        DeviceBuffer primitiveCountBuffer = DeviceBuffer(sizeof(uint32_t));
        primitiveCountBuffer.MemsetZero();
        DeviceBuffer pointCountBuffer = DeviceBuffer(sizeof(uint32_t));
        pointCountBuffer.MemsetZero();

        STData data{};
        data.voxelWorldInfo = m_VoxelWorldInfo;
        data.primitives = m_PrimitiveBuffer.DevicePointerCast<OptixAabb>();
        data.rtPoints = m_RtPointBuffer.DevicePointerCast<glm::vec3>();
        data.primitiveCount = primitiveCountBuffer.DevicePointerCast<uint32_t>();
        data.pointNodes = pointNodeBuffer.DevicePointerCast<PointNode>();
        data.voxelPointNodeIndices = voxelPointNodeIndicesBuffer.DevicePointerCast<glm::uvec2>();
        data.primitiveInfos = m_PrimitiveInfoBuffer.DevicePointerCast<IEPrimitiveInfo>();
        data.points = m_PrimitivePointBuffer.DevicePointerCast<PrimitivePoint>();
        data.pointCount = pointCountBuffer.DevicePointerCast<uint32_t>();
        data.ieCount = m_IeCount;
        data.aabbBias = aabbBias;
        
        KernelData::Get().GetStConstantBuffer().Upload(&data, 1);
        constexpr uint32_t blockSize = 32;
        uint32_t gridCount = Utils::GetLaunchCount(m_IeCount, blockSize);
        KernelData::Get().GetStCreatePrimitivesKernel().LaunchAndSynchronize(glm::uvec3(gridCount, 1, 1), glm::uvec3(blockSize, 1, 1));

        m_AccelerationStructure = AccelerationStructure::CreateFromAabbs(m_PrimitiveBuffer, m_IeCount);
        pointCountBuffer.Download(&m_PointCount, 1);
        return m_AccelerationStructure.IsValid();
    }
}
