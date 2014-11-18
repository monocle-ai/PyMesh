#include "PeriodicInflator.h"

#include <iostream>
#include <sstream>

#include <Wires/Tiler/WireTiler.h>
#include "SimpleInflator.h"

namespace PeriodicInflatorHelper {
    const std::string thickness_attr_name("internal_thickness");
}

using namespace PeriodicInflatorHelper;

void PeriodicInflator::inflate() {
    check_thickness();
    initialize_phantom_wires();
    inflate_phantom_wires();
    clip_to_center_cell();
    clean_up();
}

void PeriodicInflator::initialize_phantom_wires() {
    m_wire_network->center_at_origin();
    const size_t dim = m_wire_network->get_dim();
    VectorF bbox_min, bbox_max;
    get_center_cell_bbox(bbox_min, bbox_max);

    bbox_min.array() *= 3;
    bbox_max.array() *= 3;

    m_wire_network->clear_attributes();
    m_wire_network->add_attribute("vertex_periodic_index", true);
    m_wire_network->add_attribute("edge_periodic_index", false);
    m_wire_network->add_attribute(thickness_attr_name,
            m_thickness_type == PER_VERTEX);
    m_wire_network->set_attribute(thickness_attr_name, m_thickness);

    WireTiler tiler(m_wire_network);
    m_phantom_wires = tiler.tile_with_guide_bbox(
            bbox_min, bbox_max, VectorI::Ones(dim) * 3);
    m_phantom_wires->center_at_origin();
    assert(m_phantom_wires->has_attribute(thickness_attr_name));
}

void PeriodicInflator::inflate_phantom_wires() {
    SimpleInflator inflator(m_phantom_wires);
    const VectorF& thickness =
        m_phantom_wires->get_attribute(thickness_attr_name);

    inflator.set_thickness_type(m_thickness_type);
    inflator.set_thickness(thickness);
    inflator.set_profile(m_profile);
    inflator.inflate();

    m_phantom_vertices = inflator.get_vertices();
    m_phantom_faces = inflator.get_faces();
    update_phantom_periodic_face_sources(inflator.get_face_sources());
    //save_mesh("phantom.msh", m_phantom_vertices, m_phantom_faces,
    //        m_phantom_face_sources.cast<Float>());
}

void PeriodicInflator::update_phantom_periodic_face_sources(
        const VectorI& face_sources) {
    const size_t num_faces = face_sources.size();
    m_phantom_face_sources.resize(num_faces);

    const MatrixFr& vertex_periodic_index =
        m_phantom_wires->get_attribute("vertex_periodic_index");
    const MatrixFr& edge_periodic_index =
        m_phantom_wires->get_attribute("edge_periodic_index");

    for (size_t i=0; i<num_faces; i++) {
        int source = face_sources[i];
        if (source < 0) {
            // Edge index
            source = -source - 1;
            m_phantom_face_sources[i] = -edge_periodic_index(source, 0) - 1;
        } else if (source > 0) {
            // Vertex index
            source  = source - 1;
            m_phantom_face_sources[i] = vertex_periodic_index(source, 0) + 1;
        }
    }
}

void PeriodicInflator::get_center_cell_bbox(
        VectorF& bbox_min, VectorF& bbox_max) {
    bbox_min = m_wire_network->get_bbox_min();
    bbox_max = m_wire_network->get_bbox_max();
}
