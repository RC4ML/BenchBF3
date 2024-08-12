/*
 * Copyright (c) 2021-2022 NVIDIA CORPORATION & AFFILIATES, ALL RIGHTS RESERVED.
 *
 * This software product is a proprietary product of NVIDIA CORPORATION &
 * AFFILIATES (the "Company") and all right, title, and interest in and to the
 * software product, including all associated intellectual property rights, are
 * and shall remain exclusively with the Company.
 *
 * This software product is governed by the End User License Agreement
 * provided with the software product.
 *
 */

#include <rte_sft.h>

#include <doca_log.h>

#include "offload_rules.h"

DOCA_LOG_REGISTER(OFRLS);

#define GROUP_POST_SFT 1001        /* Group post-sft */
#define INITIAL_GROUP 0            /* Rule group 0 is reserved for initial rules */
#define RULE_INDX(port_id, ip_ver, l4_proto) ((port_id) + 2*((2*(ip_ver)) + (l4_proto)))

struct application_rules app_rules;    /* Application rules */
uint8_t supported_ip_versions[SFT_IP_VERSIONS_NUM] = {RTE_FLOW_ITEM_TYPE_IPV4, RTE_FLOW_ITEM_TYPE_IPV6};
uint8_t supported_l4_protocols[SFT_IP_PROTOCOLS_NUM] = {RTE_FLOW_ITEM_TYPE_UDP, RTE_FLOW_ITEM_TYPE_TCP};

/*
 * Create SFT state rules
 * This rule is triggered when a the packet has been classified by the SFT
 * A rule create for each SFT state, drop, hairpin (skipped) and RSS
 *
 * @port_id [in]: port id to create the rule on
 * @action_rss [in]: RSS action to apply, can be used to hairpin or to forward to SW
 * @sft_state [in]: SFT state to create the rule for
 * @error [out]: Error structure
 * @out_rule [out]: pointer to the created rule or NULL on error
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
create_rule_post_sft(uint16_t port_id, struct rte_flow_action_rss *action_rss, uint8_t sft_state,
                     struct rte_flow_error *error, struct rte_flow **out_rule) {
    int ret;
    struct rte_flow_item_sft sft_spec_and_mask = {.fid_valid = 1, .state = sft_state};
    struct rte_flow_attr attr = {
            .ingress = 1,
            .priority = SET_STATE_PRIORITY,
            .group = GROUP_POST_SFT,
    };
    struct rte_flow_action match_action = {.type = RTE_FLOW_ACTION_TYPE_RSS, .conf = action_rss};
    struct rte_flow_action drop_action = {.type = RTE_FLOW_ACTION_TYPE_DROP};
    struct rte_flow_item pattern[] = {
            [0] = {.type = RTE_FLOW_ITEM_TYPE_SFT, .mask = &sft_spec_and_mask, .spec = &sft_spec_and_mask},
            [1] = {.type = RTE_FLOW_ITEM_TYPE_END},
    };
    struct rte_flow_action action[] = {
            [0] = {.type = RTE_FLOW_ACTION_TYPE_COUNT},
            /* When calling this function, sft_state can be only HAIRPIN_MATCHED_FLOW or DROP_FLOW */
            [1] = (sft_state == DROP_FLOW) ? drop_action : match_action,
            [2] = {.type = RTE_FLOW_ACTION_TYPE_END},
    };

    ret = rte_flow_validate(port_id, &attr, pattern, action, error);
    if (ret != 0)
        return DOCA_ERROR_DRIVER;
    *out_rule = rte_flow_create(port_id, &attr, pattern, action, error);
    return *out_rule ? DOCA_SUCCESS : DOCA_ERROR_DRIVER;
}

/*
 * Create a flow rule to match the initial state of the SFT
 * This rule is used to forward IPv4/IPv6 TCP/UDP packets to the SFT
 *
 * @port_id [in]: Port id to create the rule on
 * @pattern [in]: Rule pattern to match
 * @priority [in]: Rule priority
 * @error [out]: Error structure
 * @out_rule [out]: pointer to the created rule or NULL on error
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
create_rule_forward_to_sft_by_pattern(uint8_t port_id, struct rte_flow_item **pattern, uint8_t priority,
                                      struct rte_flow_error *error, struct rte_flow **out_rule) {
    int ret;
    struct rte_flow_action_sft action_sft = {.zone = 0xcafe}; /* SFT zone is 0xcafe */
    struct rte_flow_action_jump action_jump = {.group = GROUP_POST_SFT};
    struct rte_flow_attr attr = {
            .ingress = 1,
            .priority = priority,
            .group = INITIAL_GROUP,
    };
    struct rte_flow_action action[] = {
            [0] = {.type = RTE_FLOW_ACTION_TYPE_COUNT},
            [1] = {.type = RTE_FLOW_ACTION_TYPE_SFT, .conf = &action_sft},
            [2] = {.type = RTE_FLOW_ACTION_TYPE_JUMP, .conf = &action_jump},
            [3] = {.type = RTE_FLOW_ACTION_TYPE_END}
    };
    struct rte_flow_item (*pattern_arr)[] = (struct rte_flow_item(*)[]) pattern;

    ret = rte_flow_validate(port_id, &attr, *pattern_arr, action, error);
    if (ret != 0)
        return DOCA_ERROR_DRIVER;
    *out_rule = rte_flow_create(port_id, &attr, *pattern_arr, action, error);
    return *out_rule ? DOCA_SUCCESS : DOCA_ERROR_DRIVER;
}

/*
 * Wrapper for create_rule_forward_to_sft_by_pattern() function
 * Notice that this rule also matches fragmented packets
 * (because the check is against the next protocol in the IP header)
 *
 * @port_id [in]: port id to create rule on
 * @l3_protocol [in]: l3 protocol to match (IPv4 or IPv6)
 * @l4_protocol [in]: l4 protocol to match (TCP or UDP)
 * @error [out]: pointer to error structure
 * @out_rule [out]: pointer to the created rule or NULL on error
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
create_rule_forward_l4_to_sft(uint8_t port_id, uint8_t l3_protocol, uint8_t l4_protocol,
                              struct rte_flow_error *error, struct rte_flow **out_rule) {
    struct rte_flow_item pattern[] = {
            [0] = {.type = RTE_FLOW_ITEM_TYPE_ETH},
            [1] = {.type = l3_protocol},
            [2] = {.type = l4_protocol},
            [3] = {.type = RTE_FLOW_ITEM_TYPE_END},
    };

    return create_rule_forward_to_sft_by_pattern(port_id, (struct rte_flow_item **) &pattern,
                                                 JUMP_TO_SFT_PRIORITY, error, out_rule);
}

/*
 * Wrapper for create_rule_forward_to_sft_by_pattern() function
 * This rule offloads fragmented packets to the HW, used when fragmentation is disabled.
 *
 * @port_id [in]: port id to create rule on
 * @l3_protocol [in]: l3 protocol to match (IPv4 or IPv6)
 * @l4_protocol [in]: l4 protocol to match (TCP or UDP)
 * @error [out]: pointer to error structure
 * @out_rule [out]: pointer to the created rule or NULL on error
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
create_rule_forward_fragments_to_sft(uint8_t port_id, uint8_t l3_protocol, uint8_t l4_protocol,
                                     struct rte_flow_error *error, struct rte_flow **out_rule) {
    struct rte_flow_item_ipv6 ipv6_spec_and_mask = {.has_frag_ext = 1};
    struct rte_flow_item_ipv4 ipv4_spec = {.hdr.fragment_offset = rte_cpu_to_be_16(1)};
    struct rte_flow_item_ipv4 ipv4_mask_and_last = {.hdr.fragment_offset = rte_cpu_to_be_16(0x3fff)};
    struct rte_flow_item ipv4_opt = {.type = RTE_FLOW_ITEM_TYPE_IPV4,
            .spec = &ipv4_spec,
            .last = &ipv4_mask_and_last,
            .mask = &ipv4_mask_and_last};
    struct rte_flow_item ipv6_opt = {
            .type = RTE_FLOW_ITEM_TYPE_IPV6,
            .mask = &ipv6_spec_and_mask,
            .spec = &ipv6_spec_and_mask};
    struct rte_flow_item pattern[] = {
            [0] = {.type = RTE_FLOW_ITEM_TYPE_ETH},
            [1] = (l3_protocol == RTE_FLOW_ITEM_TYPE_IPV4) ? ipv4_opt : ipv6_opt,
            [2] = {.type = l4_protocol},
            [3] = {.type = RTE_FLOW_ITEM_TYPE_END},
    };

    return create_rule_forward_to_sft_by_pattern(port_id, (struct rte_flow_item **) &pattern, FRAG_PRIORITY, error,
                                                 out_rule);
}

/*
 * Create a rule to match pattern and apply RSS action to it
 *
 * @port_id [in]: Port id to create the rule on
 * @action_rss [in]: Action to apply to the rule
 * @attr [in]: Rule attributes
 * @pattern [in]: Rule pattern
 * @error [out]: Error structure
 * @out_rule [out]: pointer to the created rule or NULL on error
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
create_rule_action_rss(uint16_t port_id, struct rte_flow_action_rss *action_rss, struct rte_flow_attr *attr,
                       struct rte_flow_item *pattern, struct rte_flow_error *error, struct rte_flow **out_rule) {
    int ret;
    struct rte_flow_action action[] = {
            [0] = {.type = RTE_FLOW_ACTION_TYPE_COUNT},
            [1] = {.type = RTE_FLOW_ACTION_TYPE_RSS, .conf = action_rss},
            [2] = {.type = RTE_FLOW_ACTION_TYPE_END}
    };

    ret = rte_flow_validate(port_id, attr, pattern, action, error);
    if (ret != 0)
        return DOCA_ERROR_DRIVER;
    *out_rule = rte_flow_create(port_id, attr, pattern, action, error);
    return *out_rule ? DOCA_SUCCESS : DOCA_ERROR_DRIVER;
}

/*
 * Create a rule to match Ethernet packets and apply RSS action to it
 *
 * @port_id [in]: Port id to create the rule on
 * @action_rss [in]: Action to apply to the rule
 * @attr [in]: Rule attributes
 * @error [out]: Error structure
 * @out_rule [out]: pointer to the created rule or NULL on error
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
create_rule_action_eth_rss(uint16_t port_id, struct rte_flow_action_rss *action_rss, struct rte_flow_attr *attr,
                           struct rte_flow_error *error, struct rte_flow **out_rule) {
    struct rte_flow_item pattern[] = {
            [0] = {.type = RTE_FLOW_ITEM_TYPE_ETH},
            [1] = {.type = RTE_FLOW_ITEM_TYPE_END},
    };

    return create_rule_action_rss(port_id, action_rss, attr, pattern, error, out_rule);
}

/*
 * Create a rule to match fragmented packets and apply RSS action to it (hairpin them, in practice)
 *
 * @port_id [in]: Port id to create the rule on
 * @l3_protocol [in]: l3 protocol to match (IPv4 or IPv6)
 * @l4_protocol [in]: l4 protocol to match (TCP or UDP)
 * @action_rss [in]: Action to apply to the rule
 * @error [out]: Error structure
 * @out_rule [out]: pointer to the created rule or NULL on error
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
create_rule_hairpin_fragments(uint8_t port_id, uint8_t l3_protocol, uint8_t l4_protocol,
                              struct rte_flow_action_rss *action_rss,
                              struct rte_flow_error *error, struct rte_flow **out_rule) {
    struct rte_flow_attr attr = {
            .ingress = 1,
            .priority = FRAG_PRIORITY,
            .group = INITIAL_GROUP,
    };
    struct rte_flow_item_ipv6 ipv6_spec_and_mask = {.has_frag_ext = 1};
    struct rte_flow_item_ipv4 ipv4_spec = {.hdr.fragment_offset = rte_cpu_to_be_16(1)};
    struct rte_flow_item_ipv4 ipv4_mask_and_last = {.hdr.fragment_offset = rte_cpu_to_be_16(0x3fff)};
    struct rte_flow_item ipv4_opt = {.type = RTE_FLOW_ITEM_TYPE_IPV4,
            .spec = &ipv4_spec,
            .last = &ipv4_mask_and_last,
            .mask = &ipv4_mask_and_last};
    struct rte_flow_item ipv6_opt = {
            .type = RTE_FLOW_ITEM_TYPE_IPV6,
            .mask = &ipv6_spec_and_mask,
            .spec = &ipv6_spec_and_mask};
    struct rte_flow_item pattern[] = {
            [0] = {.type = RTE_FLOW_ITEM_TYPE_ETH},
            [1] = (l3_protocol == RTE_FLOW_ITEM_TYPE_IPV4) ? ipv4_opt : ipv6_opt,
            [2] = {.type = l4_protocol},
            [3] = {.type = RTE_FLOW_ITEM_TYPE_END},
    };

    return create_rule_action_rss(port_id, action_rss, &attr, pattern, error, out_rule);
}

/*
 * Create a rule to hairpin non L4 packets
 *
 * @port_id [in]: Port id to create the rule on
 * @action_rss [in]: RSS Action to apply to the rule
 * @error [out]: Error structure
 * @out_rule [out]: pointer to the created rule or NULL on error
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
create_rule_hairpin_non_l4(uint16_t port_id, struct rte_flow_action_rss *action_rss,
                           struct rte_flow_error *error, struct rte_flow **out_rule) {
    struct rte_flow_attr attr = {
            .ingress = 1,
            .priority = HAIRPIN_NON_L4_PRIORITY,
            .group = INITIAL_GROUP,
    };

    return create_rule_action_eth_rss(port_id, action_rss, &attr, error, out_rule);
}

/*
 * Create a rule to rss packets to SFT
 *
 * @port_id [in]: Port id to create the rule on
 * @action_rss [in]: RSS Action to apply to the rule
 * @error [out]: Error structure
 * @out_rule [out]: pointer to the created rule or NULL on error
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
create_rule_rss_post_sft(uint16_t port_id, struct rte_flow_action_rss *action_rss,
                         struct rte_flow_error *error, struct rte_flow **out_rule) {
    struct rte_flow_attr attr = {
            .ingress = 1,
            .priority = SFT_TO_RSS_PRIORITY,
            .group = GROUP_POST_SFT,
    };

    return create_rule_action_eth_rss(port_id, action_rss, &attr, error, out_rule);
}

doca_error_t
create_rules_sft_offload(const struct application_dpdk_config *app_dpdk_config, struct rte_flow_action_rss *action_rss,
                         struct rte_flow_action_rss *action_rss_hairpin) {

    uint8_t port_id = 0;
    uint8_t ip_version_idx = 0;
    uint8_t l4_protocol_idx = 0;
    uint8_t nb_ports = app_dpdk_config->port_config.nb_ports;
    struct rte_flow_error rte_error;
    doca_error_t result;

    if (nb_ports > SFT_PORTS_NUM) {
        DOCA_LOG_ERR("Invalid SFT ports number [%u] is greater than [%d]", nb_ports, SFT_PORTS_NUM);
        return DOCA_ERROR_INVALID_VALUE;
    }

    memset(&app_rules, 0, sizeof(app_rules));

    /*
     * RTE_FLOW rules are created as list:
     * 1. Forward IPv4/6 L4 traffic to SFT with predefined zone in group 0 (Also fragments if enabled)
     * 2. Check traffic for state && valid fid for either hairpin or dropped state in the SFT group
     * 3. RSS all the L4 non-state traffic to the ARM cores
     */

    for (port_id = 0; port_id < nb_ports; port_id++) {
        for (ip_version_idx = 0; ip_version_idx < SFT_IP_VERSIONS_NUM; ip_version_idx++) {
            for (l4_protocol_idx = 0; l4_protocol_idx < SFT_IP_PROTOCOLS_NUM; l4_protocol_idx++) {
                result = create_rule_forward_l4_to_sft(port_id, supported_ip_versions[ip_version_idx],
                                                       supported_l4_protocols[l4_protocol_idx], &rte_error,
                                                       &app_rules.set_jump_to_sft_action[RULE_INDX(port_id,
                                                                                                   ip_version_idx,
                                                                                                   l4_protocol_idx)]);

                if (result != DOCA_SUCCESS) {
                    DOCA_LOG_ERR("Forward to SFT IP%d-%s failed, error=%s",
                                 supported_ip_versions[ip_version_idx] == RTE_FLOW_ITEM_TYPE_IPV4 ? 4 : 6,
                                 supported_l4_protocols[l4_protocol_idx] == RTE_FLOW_ITEM_TYPE_UDP ? "UDP" : "TCP",
                                 rte_error.message);
                    return result;
                }
            }
        }

        result = create_rule_rss_post_sft(port_id, action_rss, &rte_error, &app_rules.rss_non_state[port_id]);

        if (result != DOCA_SUCCESS) {
            DOCA_LOG_ERR("SFT set non state RSS failed, error=%s", rte_error.message);
            return result;
        }

        result = create_rule_hairpin_non_l4(port_id, action_rss_hairpin, &rte_error,
                                            &app_rules.hairpin_non_l4[port_id]);

        if (result != DOCA_SUCCESS) {
            DOCA_LOG_ERR("Hairpin flow creation failed: %s", rte_error.message);
            return result;
        }

        if (app_dpdk_config->sft_config.enable_state_hairpin) {
            result = create_rule_post_sft(port_id, action_rss_hairpin, HAIRPIN_MATCHED_FLOW, &rte_error,
                                          &app_rules.query_hairpin[port_id]);

            if (result != DOCA_SUCCESS) {
                DOCA_LOG_ERR("Forward fid with state, error=%s", rte_error.message);
                return result;
            }
        }

        if (app_dpdk_config->sft_config.enable_state_drop) {
            result = create_rule_post_sft(port_id, action_rss_hairpin, DROP_FLOW, &rte_error,
                                          &app_rules.query_hairpin[port_id + 2]);
            if (result != DOCA_SUCCESS) {
                DOCA_LOG_ERR("Drop fid with state, error=%s", rte_error.message);
                return result;
            }
        }

        if (app_dpdk_config->sft_config.enable_frag) {

            for (ip_version_idx = 0; ip_version_idx < SFT_IP_VERSIONS_NUM; ip_version_idx++) {
                for (l4_protocol_idx = 0; l4_protocol_idx < SFT_IP_PROTOCOLS_NUM; l4_protocol_idx++) {
                    result = create_rule_forward_fragments_to_sft(port_id, supported_ip_versions[ip_version_idx],
                                                                  supported_l4_protocols[l4_protocol_idx], &rte_error,
                                                                  &app_rules.forward_fragments[RULE_INDX(port_id,
                                                                                                         ip_version_idx,
                                                                                                         l4_protocol_idx)]);
                    if (result != DOCA_SUCCESS) {
                        DOCA_LOG_ERR("Fragments to SFT IP%d-%s failed, error=%s",
                                     supported_ip_versions[ip_version_idx] == RTE_FLOW_ITEM_TYPE_IPV4 ? 4 : 6,
                                     supported_l4_protocols[l4_protocol_idx] == RTE_FLOW_ITEM_TYPE_UDP ? "UDP" : "TCP",
                                     rte_error.message);
                        return result;
                    }
                }
            }
        } else {
            /* Create highest priority rules to hairpin fragments */
            for (ip_version_idx = 0; ip_version_idx < SFT_IP_VERSIONS_NUM; ip_version_idx++) {
                for (l4_protocol_idx = 0; l4_protocol_idx < SFT_IP_PROTOCOLS_NUM; l4_protocol_idx++) {
                    result = create_rule_hairpin_fragments(port_id, supported_ip_versions[ip_version_idx],
                                                           supported_l4_protocols[l4_protocol_idx], action_rss_hairpin,
                                                           &rte_error,
                                                           &app_rules.hairpin_fragments[RULE_INDX(port_id,
                                                                                                  ip_version_idx,
                                                                                                  l4_protocol_idx)]);
                    if (result != DOCA_SUCCESS) {
                        DOCA_LOG_ERR("Hairpin fragments IP%d-%s failed, error=%s",
                                     supported_ip_versions[ip_version_idx] == RTE_FLOW_ITEM_TYPE_IPV4 ? 4 : 6,
                                     supported_l4_protocols[l4_protocol_idx] == RTE_FLOW_ITEM_TYPE_UDP ? "UDP" : "TCP",
                                     rte_error.message);
                        return result;
                    }
                }
            }
        }
    }
    return DOCA_SUCCESS;
}

void
print_offload_rules_counter(void) {
    uint8_t flow_i;
    uint8_t port_id;
    struct rte_flow_action action[] = {
            [0] = {.type = RTE_FLOW_ACTION_TYPE_COUNT},
            [1] = {.type = RTE_FLOW_ACTION_TYPE_END},
    };
    struct rte_flow_query_count count = {0};
    struct rte_flow_error rte_error;
    uint64_t total_ingress = 0;
    uint64_t total_rss = 0;
    uint64_t total_dropped = 0;
    uint64_t total_egress = 0;
    uint64_t total_ingress_non_l4 = 0;
    uint64_t total_fragments = 0;

    DOCA_LOG_DBG("------------ L4 Jump to SFT----------------");
    for (flow_i = 0; flow_i < 8; flow_i++) {
        port_id = flow_i % 2; /* Even flow record = port 0,  Odd flow record = port 1 */
        if (rte_flow_query(port_id, app_rules.set_jump_to_sft_action[flow_i], &action[0], &count, &rte_error) !=
            0) {
            DOCA_LOG_ERR("Query failed, error=%s", rte_error.message);
        } else {
            DOCA_LOG_DBG("Port %d IPv%d-%s Jump to SFT - %lu", port_id, flow_i < 4 ? 4 : 6,
                         flow_i % 4 < 2 ? "UDP" : "TCP", count.hits);
            total_ingress += count.hits;
        }
    }

    DOCA_LOG_DBG("----------Hairpin non L4 traffic-----------");
    for (flow_i = 0; flow_i < 2; flow_i++) {
        port_id = flow_i;
        if (rte_flow_query(port_id, app_rules.hairpin_non_l4[flow_i], &action[0], &count, &rte_error) != 0) {
            DOCA_LOG_ERR("Query failed, error=%s", rte_error.message);
        } else {
            DOCA_LOG_DBG("Port %d non L4- %lu", port_id, count.hits);
            total_ingress += count.hits;
            total_ingress_non_l4 += count.hits;
        }
    }

    DOCA_LOG_DBG("---------Hairpin using state post SFT -----");
    for (flow_i = 0; flow_i < 4; flow_i++) {
        port_id = flow_i % 2; /* Even flow record = port 0,  Odd flow record = port 1 */
        if (app_rules.query_hairpin[flow_i] == NULL)
            continue;

        if (rte_flow_query(port_id, app_rules.query_hairpin[flow_i], &action[0], &count, &rte_error) != 0) {
            DOCA_LOG_ERR("Query failed, error=%s", rte_error.message);
        } else {
            if (flow_i < 2) {
                DOCA_LOG_DBG("Port %d state hairpin - %lu", port_id, count.hits);
                total_egress += count.hits;
            } else {
                DOCA_LOG_DBG("Port %d state drop - %lu", port_id, count.hits);
                total_dropped += count.hits;
            }
        }
    }

    DOCA_LOG_DBG("---------------RSS post SFT----------------");
    for (flow_i = 0; flow_i < 2; flow_i++) {
        port_id = flow_i;
        if (rte_flow_query(port_id, app_rules.rss_non_state[flow_i], &action[0], &count, &rte_error) != 0) {
            DOCA_LOG_ERR("Query failed, error=%s", rte_error.message);
        } else {
            DOCA_LOG_DBG("Port %d RSS to queues - %lu", port_id, count.hits);
            total_rss += count.hits;
        }
    }

    /*
     * Fragments:
     * if fragments are enabled - check counters of forwarded fragments
     * else check counters of hairpin fragments
     */

    if (app_rules.forward_fragments[0] != NULL) {
        DOCA_LOG_DBG("---------Fragments to SFT -----");
        for (flow_i = 0; flow_i < 8; flow_i++) {
            port_id = flow_i % 2; /* Even flow record = port 0,  Odd flow record = port 1 */
            if (rte_flow_query(port_id, app_rules.forward_fragments[flow_i], &action[0], &count,
                               &rte_error) != 0) {
                DOCA_LOG_ERR("Query failed, error=%s", rte_error.message);
            } else {
                DOCA_LOG_DBG("Port %d IPv%d-%s fragments - %lu", port_id, (flow_i < 4) ? 4 : 6,
                             (flow_i % 4) < 2 ? "UDP" : "TCP", count.hits);
                total_fragments += count.hits;
                total_ingress += count.hits;
            }
        }
    }

    if (app_rules.hairpin_fragments[0] != NULL) {
        DOCA_LOG_DBG("---------Hairpin fragments -----");
        for (flow_i = 0; flow_i < 8; flow_i++) {
            port_id = flow_i % 2; /* Even flow record = port 0,  Odd flow record = port 1 */
            if (rte_flow_query(port_id, app_rules.hairpin_fragments[flow_i], &action[0], &count,
                               &rte_error) != 0) {
                DOCA_LOG_ERR("Query failed, error=%s", rte_error.message);
            } else {
                DOCA_LOG_DBG("Port %d IPv%d-%s fragments - %lu", port_id, (flow_i < 4) ? 4 : 6,
                             (flow_i % 4) < 2 ? "UDP" : "TCP", count.hits);
                total_fragments += count.hits;
                total_egress += count.hits;
            }
        }
    }


    DOCA_LOG_DBG("-------------------------------------------");
    DOCA_LOG_DBG("TOTAL INGRESS TRAFFIC:%lu", total_ingress);
    DOCA_LOG_DBG("TOTAL EGRESS TRAFFIC:%lu", total_egress);
    DOCA_LOG_DBG("TOTAL RSS TRAFFIC:%lu", total_rss);
    DOCA_LOG_DBG("TOTAL INGRESS NON L4 TRAFFIC:%lu", total_ingress_non_l4);
    DOCA_LOG_DBG("TOTAL FRAGMENTED TRAFFIC:%lu", total_fragments);
    DOCA_LOG_DBG("TOTAL DROPPED TRAFFIC:%lu", total_dropped);
}
