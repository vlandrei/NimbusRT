#pragma once

#include "Nimbus/Types.hpp"
#include "Nimbus/PathStorage.hpp"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>

namespace py = pybind11;

class PathWrapper
{
public:
	PathWrapper(std::unique_ptr<Nimbus::PathStorage>&& path);

	py::array_t<float, py::array::c_style> GetTransmitters() const;
	py::array_t<float, py::array::c_style> GetReceivers() const;
	py::array_t<float, py::array::c_style> GetInteractions() const;
	py::array_t<float, py::array::c_style> GetNormals() const;
	py::array_t<uint32_t, py::array::c_style> GetLabels() const;
	py::array_t<uint32_t, py::array::c_style> GetMaterials() const;
	py::array_t<double, py::array::c_style> GetTimeDelays() const;
	py::array_t<uint32_t, py::array::c_style> GetTxIDs() const;
	py::array_t<uint32_t, py::array::c_style> GetRxIDs() const;
	py::array_t<Nimbus::PathType, py::array::c_style> GetPathTypes() const;
	py::array_t<uint8_t, py::array::c_style> GetNumInteractions() const;

private:
	Nimbus::PathStorage::PathData m_Data;
};

class CoverageWrapper : public PathWrapper
{
public:
	CoverageWrapper(std::unique_ptr<Nimbus::PathStorage>&& path, Nimbus::CoverageMapInfo&& mapInfo);

	py::array_t<uint32_t, py::array::c_style> GetRx2D() const;
	py::array_t<uint32_t, py::array::c_style> GetDimensions() const;

private:
	Nimbus::CoverageMapInfo m_CoverageMapInfo;
};

class SionnaPathWrapper
{
public:
	SionnaPathWrapper(std::unique_ptr<Nimbus::PathStorage>&& path);
	py::array_t<float, py::array::c_style> GetTransmitters() const;
	py::array_t<float, py::array::c_style> GetReceivers() const;
	py::array_t<float, py::array::c_style> GetInteractions(uint32_t sionnaPathType) const;
	py::array_t<float, py::array::c_style> GetNormals(uint32_t sionnaPathType) const;
	py::array_t<int32_t, py::array::c_style> GetMaterials(uint32_t sionnaPathType) const;
	py::array_t<float, py::array::c_style> GetIncidentRays(uint32_t sionnaPathType) const;
	py::array_t<float, py::array::c_style> GetDeflectedRays(uint32_t sionnaPathType) const;
	py::array_t<float, py::array::c_style> GetTimeDelays(uint32_t sionnaPathType) const;
	py::array_t<float, py::array::c_style> GetTotalDistance(uint32_t sionnaPathType) const;
	py::array_t<uint8_t, py::array::c_style> GetMask(uint32_t sionnaPathType) const;
	py::array_t<float, py::array::c_style> GetKTx(uint32_t sionnaPathType) const;
	py::array_t<float, py::array::c_style> GetKRx(uint32_t sionnaPathType) const;
	py::array_t<float, py::array::c_style> GetAodElevation(uint32_t sionnaPathType) const;
	py::array_t<float, py::array::c_style> GetAodAzimuth(uint32_t sionnaPathType) const;
	py::array_t<float, py::array::c_style> GetAoaElevation(uint32_t sionnaPathType) const;
	py::array_t<float, py::array::c_style> GetAoaAzimuth(uint32_t sionnaPathType) const;

private:
	Nimbus::PathStorage::SionnaPathData m_SionnaData;
};