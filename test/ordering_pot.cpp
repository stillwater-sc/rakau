// Copyright 2018 Francesco Biscani (bluescarni@gmail.com)
//
// This file is part of the rakau library.
//
// This Source Code Form is subject to the terms of the Mozilla
// Public License v. 2.0. If a copy of the MPL was not distributed
// with this file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <rakau/tree.hpp>

#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <limits>
#include <random>
#include <tuple>
#include <type_traits>
#include <vector>

#include <boost/math/constants/constants.hpp>

#include "test_utils.hpp"

using namespace rakau;
using namespace rakau::kwargs;
using namespace rakau_test;

using fp_types = std::tuple<float, double>;
using macs = std::tuple<std::integral_constant<mac, mac::bh>, std::integral_constant<mac, mac::bh_geom>>;

static std::mt19937 rng(2);

TEST_CASE("potentials ordering")
{
    tuple_for_each(macs{}, [](auto mac_type) {
        tuple_for_each(fp_types{}, [](auto x) {
            using fp_type = decltype(x);
            constexpr auto bsize = static_cast<fp_type>(10), theta = static_cast<fp_type>(.01);
            constexpr auto s = 10000u;
            // Get random particles in a box 1/10th the size of the domain.
            auto parts = get_uniform_particles<3>(s, bsize / fp_type(10), rng);
            octree<fp_type, decltype(mac_type)::value> t{x_coords = parts.begin() + s,
                                                         y_coords = parts.begin() + 2u * s,
                                                         z_coords = parts.begin() + 3u * s,
                                                         masses = parts.begin(),
                                                         nparts = s,
                                                         box_size = bsize};
            // Select randomly some particle indices to track.
            using size_type = typename decltype(t)::size_type;
            std::vector<size_type> track_idx(100);
            std::uniform_int_distribution<size_type> idist(0, s - 1u);
            std::generate(track_idx.begin(), track_idx.end(), [&idist]() { return idist(rng); });
            // Get the exact potentials on the particles.
            std::vector<fp_type> exact_pots;
            for (auto idx : track_idx) {
                exact_pots.emplace_back(t.exact_pot_o(idx));
            }
            // Now let's rotate the particles around the z axis by a random amount.
            std::uniform_real_distribution<fp_type> urd(fp_type(0), fp_type(2) * boost::math::constants::pi<fp_type>());
            auto rot_angle = urd(rng);
            t.update_particles_u([rot_angle](const auto &r) {
                for (auto i = 0u; i < s; ++i) {
                    const auto x0 = r[0][i];
                    const auto y0 = r[1][i];
                    const auto z0 = r[2][i];
                    const auto r0 = std::hypot(x0, y0, z0);
                    const auto th0 = std::acos(z0 / r0);
                    const auto phi0 = std::atan2(y0, x0);
                    const auto phi1 = phi0 + rot_angle;
                    r[0][i] = r0 * std::sin(th0) * std::cos(phi1);
                    r[1][i] = r0 * std::sin(th0) * std::sin(phi1);
                    r[2][i] = r0 * std::cos(th0);
                }
            });
            // Now let's get the tree-calculated potentials.
            std::vector<fp_type> t_pots;
            t.pots_o(t_pots, theta);
            // Let's compare the potentials.
            for (size_type i = 0; i < track_idx.size(); ++i) {
                const auto epot = exact_pots[i];
                const auto tpot = t_pots[track_idx[i]];
                const auto rdiff = std::abs((epot - tpot) / epot);
                std::cout << "rdiff=" << rdiff << '\n';
                if (std::numeric_limits<fp_type>::is_iec559) {
                    // The usual arbitrary values.
                    if (std::is_same_v<fp_type, double>) {
                        REQUIRE(rdiff <= fp_type(2E-11));
                    } else {
                        REQUIRE(rdiff <= fp_type(2E-3));
                    }
                }
            }
            // Do another rotation and re-check.
            rot_angle = urd(rng);
            t.update_particles_u([rot_angle](const auto &r) {
                for (auto i = 0u; i < s; ++i) {
                    const auto x0 = r[0][i];
                    const auto y0 = r[1][i];
                    const auto z0 = r[2][i];
                    const auto r0 = std::hypot(x0, y0, z0);
                    const auto th0 = std::acos(z0 / r0);
                    const auto phi0 = std::atan2(y0, x0);
                    const auto phi1 = phi0 + rot_angle;
                    r[0][i] = r0 * std::sin(th0) * std::cos(phi1);
                    r[1][i] = r0 * std::sin(th0) * std::sin(phi1);
                    r[2][i] = r0 * std::cos(th0);
                }
            });
            // Try the other overload as well.
            t.pots_o(t_pots.begin(), theta);
            for (size_type i = 0; i < track_idx.size(); ++i) {
                const auto epot = exact_pots[i];
                const auto tpot = t_pots[track_idx[i]];
                const auto rdiff = std::abs((epot - tpot) / epot);
                std::cout << "rdiff=" << rdiff << '\n';
                if (std::numeric_limits<fp_type>::is_iec559) {
                    if (std::is_same_v<fp_type, double>) {
                        REQUIRE(rdiff <= fp_type(2E-11));
                    } else {
                        REQUIRE(rdiff <= fp_type(2E-3));
                    }
                }
            }
            // Add a small delta to all coordinates and re-check.
            std::uniform_real_distribution<fp_type> urd2(fp_type(0), fp_type(3));
            auto delta = urd2(rng);
            t.update_particles_u([delta](const auto &r) {
                for (auto i = 0u; i < s; ++i) {
                    r[0][i] += delta;
                    r[1][i] += delta;
                    r[2][i] += delta;
                }
            });
            t.pots_o(t_pots, theta);
            for (size_type i = 0; i < track_idx.size(); ++i) {
                const auto epot = exact_pots[i];
                const auto tpot = t_pots[track_idx[i]];
                const auto rdiff = std::abs((epot - tpot) / epot);
                std::cout << "rdiff=" << rdiff << '\n';
                if (std::numeric_limits<fp_type>::is_iec559) {
                    if (std::is_same_v<fp_type, double>) {
                        REQUIRE(rdiff <= fp_type(2E-11));
                    } else {
                        REQUIRE(rdiff <= fp_type(2E-3));
                    }
                }
            }
            // Now let's update the exact potentials on the tracked particles.
            for (size_type i = 0; i < track_idx.size(); ++i) {
                exact_pots[i] = t.exact_pot_o(track_idx[i]);
            }
            // Let's remove the delta.
            t.update_particles_u([delta](const auto &r) {
                for (auto i = 0u; i < s; ++i) {
                    r[0][i] -= delta;
                    r[1][i] -= delta;
                    r[2][i] -= delta;
                }
            });
            // Compute the tree potentials.
            t.pots_o(t_pots, theta);
            // Verify they are close to the original ones.
            for (size_type i = 0; i < track_idx.size(); ++i) {
                const auto epot = exact_pots[i];
                const auto rdiff = std::abs((epot - t_pots[track_idx[i]]) / epot);
                if (std::numeric_limits<fp_type>::is_iec559) {
                    if (std::is_same_v<fp_type, double>) {
                        REQUIRE(rdiff <= fp_type(2E-11));
                    } else {
                        REQUIRE(rdiff <= fp_type(2E-3));
                    }
                }
            }
        });
    });
}
