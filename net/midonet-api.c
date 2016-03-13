// -*- mode: C; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil -*-
// vim: set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:

/*************************************************************************
 * Copyright 2009-2012 Eucalyptus Systems, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see http://www.gnu.org/licenses/.
 *
 * Please contact Eucalyptus Systems, Inc., 6755 Hollister Ave., Goleta
 * CA 93117, USA or visit http://www.eucalyptus.com/licenses/ if you need
 * additional information or have any questions.
 *
 * This file may incorporate work covered under the following copyright
 * and permission notice:
 *
 *   Software License Agreement (BSD License)
 *
 *   Copyright (c) 2008, Regents of the University of California
 *   All rights reserved.
 *
 *   Redistribution and use of this software in source and binary forms,
 *   with or without modification, are permitted provided that the
 *   following conditions are met:
 *
 *     Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *     Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer
 *     in the documentation and/or other materials provided with the
 *     distribution.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *   FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *   COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *   INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *   BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *   LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *   CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *   ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *   POSSIBILITY OF SUCH DAMAGE. USERS OF THIS SOFTWARE ACKNOWLEDGE
 *   THE POSSIBLE PRESENCE OF OTHER OPEN SOURCE LICENSED MATERIAL,
 *   COPYRIGHTED MATERIAL OR PATENTED MATERIAL IN THIS SOFTWARE,
 *   AND IF ANY SUCH MATERIAL IS DISCOVERED THE PARTY DISCOVERING
 *   IT MAY INFORM DR. RICH WOLSKI AT THE UNIVERSITY OF CALIFORNIA,
 *   SANTA BARBARA WHO WILL THEN ASCERTAIN THE MOST APPROPRIATE REMEDY,
 *   WHICH IN THE REGENTS' DISCRETION MAY INCLUDE, WITHOUT LIMITATION,
 *   REPLACEMENT OF THE CODE SO IDENTIFIED, LICENSING OF THE CODE SO
 *   IDENTIFIED, OR WITHDRAWAL OF THE CODE CAPABILITY TO THE EXTENT
 *   NEEDED TO COMPLY WITH ANY SUCH LICENSES OR RIGHTS.
 ************************************************************************/

//!
//! @file net/midonet-api.c
//! Need description
//!

/*----------------------------------------------------------------------------*\
 |                                                                            |
 |                                  INCLUDES                                  |
 |                                                                            |
\*----------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>
#include <dirent.h>
#include <errno.h>
#include <curl/curl.h>
#include <json/json.h>

#include <eucalyptus.h>
#include <misc.h>
#include <euca_string.h>
#include <log.h>
#include <hash.h>
#include <math.h>
#include <http.h>
#include <config.h>
#include <sequence_executor.h>
#include <atomic_file.h>
#include <log.h>

#include "ipt_handler.h"
#include "ips_handler.h"
#include "ebt_handler.h"
#include "euca_gni.h"
#include "midonet-api.h"
#include "euca-to-mido.h"

/*----------------------------------------------------------------------------*\
 |                                                                            |
 |                                  DEFINES                                   |
 |                                                                            |
\*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*\
 |                                                                            |
 |                                  TYPEDEFS                                  |
 |                                                                            |
\*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*\
 |                                                                            |
 |                                ENUMERATIONS                                |
 |                                                                            |
\*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*\
 |                                                                            |
 |                                 STRUCTURES                                 |
 |                                                                            |
\*----------------------------------------------------------------------------*/

struct mem_params_t {
    char *mem;
    size_t size;
};

/*----------------------------------------------------------------------------*\
 |                                                                            |
 |                             EXTERNAL VARIABLES                             |
 |                                                                            |
\*----------------------------------------------------------------------------*/

/* Should preferably be handled in header file */

/*----------------------------------------------------------------------------*\
 |                                                                            |
 |                              GLOBAL VARIABLES                              |
 |                                                                            |
\*----------------------------------------------------------------------------*/

int midonet_api_dirty_cache = 0;
int http_gets = 0;
int http_posts = 0;
int http_puts = 0;
int http_deletes = 0;

static int http_gets_prev = 0;
static int http_posts_prev = 0;
static int http_puts_prev = 0;
static int http_deletes_prev = 0;

//!
//!
//!
//! @param[in] name
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
void mido_print_midoname(midoname * name) {
    if (name == NULL) {
        LOGWARN("Invalid argument: NULL midoname\n");
    } else {
        LOGDEBUG("init=%d tenant=%s name=%s uuid=%s resource_type=%s content_type=%s vers=%s\n",
                name->init, SP(name->tenant), SP(name->name), SP(name->uuid), SP(name->resource_type),
                SP(name->content_type), SP(name->vers));
    }
}

//!
//! Logs the API HTTP request counts (diff from a previous call)
//!
//! @see mido_info_http_count_total() for cumulative count.
//!
void mido_info_http_count()
{
    LOGINFO("MidoNet API requests: %d gets, %d puts, %d posts, %d deletes\n", 
            http_gets - http_gets_prev, http_puts - http_puts_prev,
            http_posts - http_posts_prev, http_deletes - http_deletes_prev);
    http_gets_prev = http_gets;
    http_puts_prev = http_puts;
    http_posts_prev = http_posts;
    http_deletes_prev = http_deletes;
}

//!
//! Logs the API HTTP request counts (cumulative count)
//!
//! @see mido_info_http_count() for counts between calls.
//!
void mido_info_http_count_total()
{
    LOGINFO("Total MidoNet API requests: %d gets, %d puts, %d posts, %d deletes\n", 
            http_gets, http_puts, http_posts, http_deletes);
}

//!
//! Clears the mido_parsed_route structure in the argument. Allocated memory is released.
//!
//! @param[in] route parsed route entry of interest
//!
void mido_free_mido_parsed_route(mido_parsed_route *route) {
    if (!route) {
        return;
    }
    mido_free_midoname(&(route->router));
    mido_free_midoname(&(route->rport));
    EUCA_FREE(route->src_net);
    EUCA_FREE(route->src_length);
    EUCA_FREE(route->dst_net);
    EUCA_FREE(route->dst_length);
    EUCA_FREE(route->next_hop_ip);
    EUCA_FREE(route->weight);
    bzero(route, sizeof(mido_parsed_route));
}

//!
//! Clears the list of mido_parsed_route structures in the argument.
//!
//! @param[in] routes array of mido_parsed_route structures
//! @param[in] max_routes number of array elements
//!
void mido_free_mido_parsed_route_list(mido_parsed_route *routes, int max_routes) {
    int i = 0;
    if (!routes) return;
    for (i = 0; i < max_routes; i++) {
        mido_free_mido_parsed_route(&(routes[i]));
    }
}

//!
//!
//!
//! @param[in] name
//! @param[in] max_name
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
void mido_free_midoname_list(midoname * name, int max_name)
{
    int i = 0;
    //    return;
    for (i = 0; i < max_name; i++) {
        mido_free_midoname(&(name[i]));
    }
}

//!
//!
//!
//! @param[in] name
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
void mido_free_midoname(midoname * name)
{

    if (!name) {
        return;
    }
    EUCA_FREE(name->name);
    EUCA_FREE(name->uuid);
    EUCA_FREE(name->tenant);
    EUCA_FREE(name->jsonbuf);
    EUCA_FREE(name->resource_type);
    EUCA_FREE(name->content_type);
    EUCA_FREE(name->vers);
    EUCA_FREE(name->uri);
    bzero(name, sizeof(midoname));
}

//!
//!
//!
//! @param[in]  name
//! @param[in]  key
//! @param[out] val
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_getel_midoname(midoname * name, char *key, char **val)
{
    int ret = 0;
    json_object *jobj = NULL;

    if (!name || !key || !val || (!name->init)) {
        return (1);
    }

    *val = NULL;
    jobj = json_tokener_parse(name->jsonbuf);
    if (jobj) {
        json_object_object_foreach(jobj, elkey, elval) {
            if (!*val && elkey && elval) {
                if (!strcmp(elkey, key)) {
                    *val = strdup(SP(json_object_get_string(elval)));
                }
            }
        }
        json_object_put(jobj);
    }

    if (*val == NULL) {
        ret = 1;
    }

    return (ret);
}

//!
//! Parses an json array that is a value of the given key, and returns as an array
//! of strings.
//!
//! @param[in]  name midoname containing the jsonbuf of interest.
//! @param[in]  key json key of interest.
//! @param[out] values array of parsed strings.
//! @param[out] max_values number of elements in the returning array.
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note caller is responsible for releasing memory allocated for results.
//!
int mido_getarr_midoname(midoname * name, char *key, char ***values, int *max_values)
{
    int ret = 0;
    json_object *jobj = NULL;
    json_object *jarr = NULL;
    json_object *jarrel = NULL;
    int jarr_len = 0;
    char **res;

    if (!name || !key || !values || !max_values || (!name->init)) {
        LOGWARN("Invalid argument: NULL pointer.\n");
        return (1);
    }
    LOGTRACE("searching for %s", key);

    *values = NULL;
    *max_values = 0;
    jobj = json_tokener_parse(name->jsonbuf);
    if (jobj) {
        json_object_object_get_ex(jobj, key, &jarr);
        if ((jarr == NULL) || (!json_object_is_type(jarr, json_type_array))) {
            ret = 1;
        } else {
            jarr_len = json_object_array_length(jarr);
            LOGTRACE("\tfound %d\n", jarr_len);
            if (jarr_len > 0) {
                res = EUCA_ZALLOC(jarr_len, sizeof (char *));
                if (res == NULL) {
                    LOGFATAL("out of memory.\n");
                    return (1);
                }
                for (int i = 0; i < jarr_len; i++) {
                    jarrel = json_object_array_get_idx(jarr, i);
                    res[i] = strdup(json_object_get_string(jarrel));
                    LOGTRACE("\t%d %s\n", i, res[i]);
                }
                *values = res;
                *max_values = jarr_len;
            }
        }
        json_object_put(jobj);
    }

    return (ret);
}

//!
//! Retrieves extant tunnel-zones from MidoNet NSDB.
//!
//! @param[in]  tenant name of the tenant.
//! @param[out] outnames array of midoname data structures with tunnel-zones.
//! @param[out] outnames_max number of tunnel-zones found.
//!
//! @return 0 on success, 1 otherwise.
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_get_tunnelzones(char *tenant, midoname **outnames, int *outnames_max)
{
    return (mido_get_resources(NULL, 0, tenant, "tunnel_zones", "application/vnd.org.midonet.collection.TunnelZone-v1+json", outnames, outnames_max));
}

//!
//! Retrieves hosts that are member of the given GRE tunnel-zone.
//!
//! @param[in]  tzone tunnel-zone of interest.
//! @param[out] outnames array of midoname data structures with tunnel-zone members.
//! @param[out] outnames_max number of tunnel-zone members found.
//!
//! @return 0 on success, 1 otherwise.
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_get_tunnelzone_hosts(midoname *tzone, midoname **outnames, int *outnames_max)
{
    return (mido_get_resources(tzone, 1, tzone->tenant, "hosts", "application/vnd.org.midonet.collection.GreTunnelZoneHost-v1+json", outnames, outnames_max));
}

//!
//!
//!
//! @param[in]  tenant
//! @param[in]  name
//! @param[out] outname
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_create_router(char *tenant, char *name, midoname * outname)
{
    int rc;
    midoname myname;

    bzero(&myname, sizeof(midoname));
    myname.tenant = strdup(tenant);
    myname.name = strdup(name);
    myname.resource_type = strdup("routers");
    myname.content_type = strdup("Router");

    rc = mido_create_resource(NULL, 0, &myname, outname, "name", myname.name, NULL);

    mido_free_midoname(&myname);
    return (rc);
}

/*
//!
//!
//!
//! @param[in] name
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_read_router(midoname * name)
{
    return (mido_read_resource("routers", name, NULL));
}
*/

//!
//!
//!
//! @param[in] name
//! @param[in] ... variable argument section
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_update_router(midoname * name, ...)
{
    va_list al = { {0} };
    int ret = 0;
    va_start(al, name);
    ret = mido_update_resource("routers", "Router", "v1", name, &al);
    va_end(al);

    return (ret);
}

//!
//!
//!
//! @param[in] name
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_print_router(midoname * name)
{
    return (mido_print_resource("routers", name));
}

//!
//!
//!
//! @param[in] name
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_delete_router(midoname * name)
{
    return (mido_delete_resource(NULL, name));
}

//!
//!
//!
//! @param[in]  tenant
//! @param[in]  name
//! @param[out] outname
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_create_bridge(char *tenant, char *name, midoname * outname)
{
    int rc;
    midoname myname;

    bzero(&myname, sizeof(midoname));
    myname.tenant = strdup(tenant);
    myname.name = strdup(name);
    myname.resource_type = strdup("bridges");
    myname.content_type = strdup("Bridge");

    rc = mido_create_resource(NULL, 0, &myname, outname, "name", myname.name, NULL);

    mido_free_midoname(&myname);
    return (rc);
}

/*
//!
//!
//!
//! @param[in] name
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_read_bridge(midoname * name)
{
    return (mido_read_resource("bridges", name, NULL));
}
*/

//!
//!
//!
//! @param[in] name
//! @param[in] ... variable argument section
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_update_bridge(midoname * name, ...)
{
    va_list al = { {0} };
    int ret = 0;
    va_start(al, name);
    ret = mido_update_resource("bridges", "Bridge", "v1", name, &al);
    va_end(al);

    return (ret);
}

//!
//!
//!
//! @param[in] name
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_print_bridge(midoname * name)
{
    return (mido_print_resource("bridges", name));
}

//!
//!
//!
//! @param[in] name
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_delete_bridge(midoname * name)
{
    return (mido_delete_resource(NULL, name));
}

int mido_create_portgroup(char *tenant, char *name, midoname *outname) {
    int rc = 0, found = 0;
    midoname myname;

    bzero(&myname, sizeof(midoname));
    myname.tenant = strdup(tenant);
    myname.name = strdup(name);
    myname.resource_type = strdup("port_groups");
    myname.content_type = strdup("PortGroup");
    myname.vers = strdup("v1");

    found = 0;
    if (!found) {
        rc = mido_create_resource(NULL, 0, &myname, outname, "name", myname.name, "stateful", "true", NULL);
    } else {
        rc = 0;
    }

    mido_free_midoname(&myname);
    return (rc);    
}

int mido_update_portgroup(midoname * name, ...) {
    va_list al = { {0} };
    int ret = 0;
    va_start(al, name);
    ret = mido_update_resource("port_groups", "PortGroup", "v1", name, &al);
    va_end(al);

    return (ret);
}

int mido_delete_portgroup(midoname * name) {
    return (mido_delete_resource(NULL, name));
}

int mido_print_portgroup(midoname * name) {
    int ret = 0;
    return (ret);
}

int mido_get_portgroups(char *tenant, midoname ** outnames, int *outnames_max) {
    return (mido_get_resources(NULL, 0, tenant, "port_groups", "application/vnd.org.midonet.collection.PortGroup-v1+json", outnames, outnames_max));
}

int mido_create_portgroup_port(mido_config *mido, midoname * portgroup, char *portId, midoname * outname) {
    int rc = 0, ret = 0, max_ports = 0, found = 0, i = 0;
    midoname myname, **ports = NULL;

    bzero(&myname, sizeof(midoname));

    myname.tenant = strdup(portgroup->tenant);
    myname.resource_type = strdup("ports");
    myname.content_type = strdup("PortGroupPort");
    myname.vers = strdup("v1");

    mido_resource_portgroup *cportgroup = find_mido_portgroup(mido, portgroup->name);
    if (cportgroup == NULL) {
        //Likely a newly created portgroup
        ports = NULL;
        max_ports = 0;
    } else {
        ports = cportgroup->ports;
        max_ports = cportgroup->max_ports;
    }
    //rc = mido_get_portgroup_ports(portgroup, &ports, &max_ports);
    //if (!rc) {
        found = 0;
        for (i = 0; i < max_ports && !found; i++) {
            //rc = mido_cmp_midoname_to_input(ports[i], "portId", portId, NULL);
            rc = strcmp(ports[i]->uuid, portId);
            if (!rc) {
                if (outname) {
                    mido_copy_midoname(outname, ports[i]);
                }
                found = 1;
            }
        }
    //}
    //if (ports && (max_ports > 0)) {
    //    mido_free_midoname_list(ports, max_ports);
    //    EUCA_FREE(ports);
    //}
    if (!found) {
        rc = mido_create_resource(portgroup, 1, &myname, outname, "portId", portId, NULL);
        if (rc) {
            ret = 1;
        }
    }
    
    mido_free_midoname(&myname);
    return (ret);
}

int mido_delete_portgroup_port(midoname * name) {
    return (mido_delete_resource(NULL, name));
}

int mido_get_portgroup_ports(midoname * portgroup, midoname ** outnames, int *outnames_max) {
    return (mido_get_resources(portgroup, 1, portgroup->tenant, "ports", "application/vnd.org.midonet.collection.PortGroupPort-v1+json", outnames, outnames_max));
}


//!
//!
//!
//! @param[in]  tenant
//! @param[in]  name
//! @param[out] outname
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_create_ipaddrgroup(char *tenant, char *name, midoname * outname)
{
    int rc = 0, found = 0;
    midoname myname;
    
    bzero(&myname, sizeof(midoname));
    myname.tenant = strdup(tenant);
    myname.name = strdup(name);
    myname.resource_type = strdup("ip_addr_groups");
    myname.content_type = strdup("IpAddrGroup");

    found=0;
    if (!found) {
        rc = mido_create_resource(NULL, 0, &myname, outname, "name", myname.name, NULL);
    } else {
        rc = 0;
    }

    mido_free_midoname(&myname);
    return (rc);
}

/*
//!
//!
//!
//! @param[in] name
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_read_ipaddrgroup(midoname * name)
{
    return (mido_read_resource("ip_addr_groups", name, NULL));
}
*/

//!
//!
//!
//! @param[in] name
//! @param[in] ... variable argument section
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_update_ipaddrgroup(midoname * name, ...)
{
    va_list al = { {0} };
    int ret = 0;
    va_start(al, name);
    ret = mido_update_resource("ip_addr_groups", "IpAddrGroup", "v1", name, &al);
    va_end(al);

    return (ret);
}

//!
//!
//!
//! @param[in] name
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_delete_ipaddrgroup(midoname * name)
{
    return (mido_delete_resource(NULL, name));
}

//!
//!
//!
//! @param[in] name
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_print_ipaddrgroup(midoname * name)
{
    int ret = 0;
    return (ret);
}

//!
//!
//!
//! @param[in]  tenant
//! @param[out] outnames
//! @param[out] outnames_max
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_get_ipaddrgroups(char *tenant, midoname ** outnames, int *outnames_max)
{
    return (mido_get_resources(NULL, 0, tenant, "ip_addr_groups", "application/vnd.org.midonet.collection.IpAddrGroup-v1+json", outnames, outnames_max));
}

//!
//!
//!
//! @param[in]  devname
//! @param[in]  subnet
//! @param[in]  slashnet
//! @param[in]  gw
//! @param[in]  dnsServers
//! @param[in]  max_dnsServers
//! @param[out] outname
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_create_dhcp(midoname * devname, char *subnet, char *slashnet, char *gw, u32 * dnsServers, int max_dnsServers, midoname * outname)
{
    int rc;
    midoname myname;
    char *da = NULL, *db = NULL, *dc = NULL;

    bzero(&myname, sizeof(midoname));
    myname.tenant = strdup(devname->tenant);
    myname.resource_type = strdup("dhcp");
    myname.content_type = strdup("DhcpSubnet");

    // TODO this is not so great - need to re-work the resource interface to allow for arbitrary sized arrays
    switch (max_dnsServers) {
    case 1:
        da = hex2dot(dnsServers[0]);
        rc = mido_create_resource(devname, 1, &myname, outname, "subnetPrefix", subnet, "subnetLength", slashnet, "defaultGateway", gw, "dnsServerAddrs", "jsonarr",
                                  "dnsServerAddrs:", da, "dnsServerAddrs:END", "END", NULL);
        break;
    case 2:
        da = hex2dot(dnsServers[0]);
        db = hex2dot(dnsServers[1]);
        rc = mido_create_resource(devname, 1, &myname, outname, "subnetPrefix", subnet, "subnetLength", slashnet, "defaultGateway", gw, "dnsServerAddrs", "jsonarr",
                                  "dnsServerAddrs:", da, "dnsServerAddrs:", db, "dnsServerAddrs:END", "END", NULL);
        break;
    case 3:
        da = hex2dot(dnsServers[0]);
        db = hex2dot(dnsServers[1]);
        dc = hex2dot(dnsServers[2]);
        rc = mido_create_resource(devname, 1, &myname, outname, "subnetPrefix", subnet, "subnetLength", slashnet, "defaultGateway", gw, "dnsServerAddrs", "jsonarr",
                                  "dnsServerAddrs:", da, "dnsServerAddrs:", db, "dnsServerAddrs:", dc, "dnsServerAddrs:END", "END", NULL);
        break;
    default:
        rc = mido_create_resource(devname, 1, &myname, outname, "subnetPrefix", subnet, "subnetLength", slashnet, "defaultGateway", gw, "dnsServerAddrs", "jsonarr",
                                  "dnsServerAddrs:", "8.8.8.8", "dnsServerAddrs:END", "END", NULL);
        break;
    }
    EUCA_FREE(da);
    EUCA_FREE(db);
    EUCA_FREE(dc);
    mido_free_midoname(&myname);
    return (rc);
}

/*
//!
//!
//!
//! @param[in] name
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_read_dhcp(midoname * name)
{
    return (mido_read_resource("dhcp", name, NULL));
}
*/

//!
//!
//!
//! @param[in] name
//! @param[in] ... variable argument section
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_update_dhcp(midoname * name, ...)
{
    va_list al = { {0} };
    int ret = 0;
    va_start(al, name);
    ret = mido_update_resource("dhcp", "DhcpSubnet", "v1", name, &al);
    va_end(al);

    return (ret);
}

//!
//!
//!
//! @param[in] name
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_print_dhcp(midoname * name)
{
    return (mido_print_resource("dhcp", name));
}

//!
//!
//!
//! @param[in] devname
//! @param[in] name
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_delete_dhcp(midoname * devname, midoname * name)
{
    return (mido_delete_resource(devname, name));
}

//!
//!
//!
//! @param[in]  devname
//! @param[out] outnames
//! @param[out] outnames_max
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_get_dhcps(midoname * devname, midoname ** outnames, int *outnames_max)
{
    return (mido_get_resources(devname, 1, devname->tenant, "dhcp", "application/vnd.org.midonet.collection.DhcpSubnet-v2+json", outnames, outnames_max));
}

//!
//!
//!
//! @param[in]  devname
//! @param[in]  dhcp
//! @param[out] outnames
//! @param[out] outnames_max
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_get_dhcphosts(midoname * devname, midoname * dhcp, midoname ** outnames, int *outnames_max)
{
    int rc = 0;
    midoname *parents = NULL;
    parents = calloc(2, sizeof(midoname));

    mido_copy_midoname(&(parents[0]), devname);
    mido_copy_midoname(&(parents[1]), dhcp);
    rc = mido_get_resources(parents, 2, devname->tenant, "hosts", "application/vnd.org.midonet.collection.DhcpHost-v2+json", outnames, outnames_max);
    mido_free_midoname_list(parents, 2);
    EUCA_FREE(parents);

    return (rc);
}

//!
//!
//!
//! @param[in]  devname
//! @param[in]  dhcp
//! @param[in]  name
//! @param[in]  mac
//! @param[in]  ip
//! @param[out] outname
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_create_dhcphost(midoname * devname, midoname * dhcp, char *name, char *mac, char *ip, char *dns_domain, midoname * outname)
{
    int rc = 0, ret = 0, found = 0;
    midoname myname;
    midoname *parents = NULL;

    bzero(&myname, sizeof(midoname));
    myname.name = strdup(name);
    myname.tenant = strdup(devname->tenant);
    myname.resource_type = strdup("hosts");
    myname.content_type = strdup("DhcpHost");
    myname.vers = strdup("v2");

    parents = calloc(2, sizeof(midoname));

    mido_copy_midoname(&(parents[0]), devname);
    mido_copy_midoname(&(parents[1]), dhcp);

    found=0;
    if (!found) {
        if (dns_domain) {
            rc = mido_create_resource(parents, 2, &myname, outname, "name", myname.name, "macAddr", mac, "ipAddr", ip, "extraDhcpOpts", "jsonlist", "extraDhcpOpts:optName", "domain_search", "extraDhcpOpts:optValue", dns_domain, "extraDhcpOpts:END", "END", NULL);
        } else {
            rc = mido_create_resource(parents, 2, &myname, outname, "name", myname.name, "macAddr", mac, "ipAddr", ip, NULL);
        }

        if (rc) {
            ret = 1;
        }
    }

    mido_free_midoname(&(parents[0]));
    mido_free_midoname(&(parents[1]));
    mido_free_midoname(&myname);
    EUCA_FREE(parents);
    return (ret);
}

//!
//!
//!
//! @param[in] name
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_delete_dhcphost(midoname * name)
{
    return (mido_delete_resource(NULL, name));
}

//!
//!
//!
//! @param[in]  tenant
//! @param[in]  name
//! @param[out] outname
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_create_chain(char *tenant, char *name, midoname * outname)
{
    int rc = 0, found = 0;
    midoname myname;

    bzero(&myname, sizeof(midoname));
    myname.tenant = strdup(tenant);
    myname.name = strdup(name);
    myname.resource_type = strdup("chains");
    myname.content_type = strdup("Chain");

    found=0;
    if (!found) {
        rc = mido_create_resource(NULL, 0, &myname, outname, "name", myname.name, NULL);
    }

    mido_free_midoname(&myname);
    return (rc);
}

/*
//!
//!
//!
//! @param[in] name
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_read_chain(midoname * name)
{
    return (mido_read_resource("chains", name, NULL));
}
*/

//!
//!
//!
//! @param[in] name
//! @param[in] ... variable argument section
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_update_chain(midoname * name, ...)
{
    va_list al = { {0} };
    int ret = 0;
    va_start(al, name);
    ret = mido_update_resource("chains", "Chain", "v1", name, &al);
    va_end(al);

    return (ret);
}

//!
//!
//!
//! @param[in] name
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_print_chain(midoname * name)
{
    return (mido_print_resource("chains", name));
}

//!
//!
//!
//! @param[in] name
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_delete_chain(midoname * name)
{
    return (mido_delete_resource(NULL, name));
}

//!
//! Adds an IP address to the ip-address-group specified in the argument.
//!
//! @param[in]  mido data structure holding current eucanetd MidoNet config.
//! @param[in]  ipaddrgroup ip-address-group of interest.
//! @param[in]  ip IP address to add to the specified ip-addrress-group.
//! @param[out] outname newly created ip-address-group membership information.
//! @return 0 if the membership is successfully created. 1 otherwise.
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_create_ipaddrgroup_ip(mido_config *mido, midoname *ipaddrgroup, char *ip, midoname *outname)
{
    int rc = 0, ret = 0, max_ips = 0, found = 0, i = 0;
    midoname myname, *ips = NULL;

    if (!mido || !ipaddrgroup) {
        return (1);
    }
    bzero(&myname, sizeof(midoname));
    myname.tenant = strdup(ipaddrgroup->tenant);
    myname.resource_type = strdup("ip_addrs");
    myname.content_type = strdup("IpAddrGroupAddr");

    // check if IP is already a member
    mido_resource_ipaddrgroup *cipaddrgroup = find_mido_ipaddrgroup(mido, ipaddrgroup->name);
    if (cipaddrgroup == NULL) {
        // Likely a newly created ip-address-group
        ips = NULL;
        max_ips = 0;
    } else {
        ips = cipaddrgroup->ips;
        max_ips = cipaddrgroup->max_ips;
    }
    //rc = mido_get_ipaddrgroup_ips(ipaddrgroup, &ips, &max_ips);
    //if (!rc) {
        found = 0;
        for (i = 0; i < max_ips && !found; i++) {
            if (ips[i].init == 0) {
                // skip deleted entries
                continue;
            }
            rc = mido_cmp_midoname_to_input(&(ips[i]), "addr", ip, NULL);
            if (!rc) {
                if ((outname) && (outname != &(ips[i]))) {
                    mido_copy_midoname(outname, &(ips[i]));
                }
                found = 1;
            }
        }
    //}
    //if (ips && (max_ips > 0)) {
    //    mido_free_midoname_list(ips, max_ips);
    //    EUCA_FREE(ips);
    //}

    if (!found) {
        LOGINFO("\tadding %s to %s\n", ip, ipaddrgroup->name);
        rc = mido_create_resource(ipaddrgroup, 1, &myname, outname, "addr", ip, "version", "4", NULL);
        if (rc) {
            ret = 1;
        }
    }

    mido_free_midoname(&myname);
    return (ret);
}

//!
//!
//!
//! @param[in] ipaddrgroup
//! @param[in] name
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_delete_ipaddrgroup_ip(midoname * ipaddrgroup, midoname * name)
{
    return (mido_delete_resource(ipaddrgroup, name));
}

//!
//!
//!
//! @param[in]  ipaddrgroup
//! @param[out] outnames
//! @param[out] outnames_max
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_get_ipaddrgroup_ips(midoname * ipaddrgroup, midoname ** outnames, int *outnames_max)
{
    return (mido_get_resources(ipaddrgroup, 1, ipaddrgroup->tenant, "ip_addrs", "application/vnd.org.midonet.collection.IpAddrGroupAddr-v1+json", outnames, outnames_max));
}

/*
//!
//!
//!
//! @param[in]  position
//! @param[in]  type
//! @param[in]  action
//! @param[in]  protocol
//! @param[in]  srcIAGuuid
//! @param[in]  src_port_min
//! @param[in]  src_port_max
//! @param[in]  dstIAGuuid
//! @param[in]  dst_port_min
//! @param[in]  dst_port_max
//! @param[in]  matchForwardFlow
//! @param[in]  matchReturnFlow
//! @param[in]  nat_target
//! @param[in]  nat_port_min
//! @param[in]  nat_port_max
//! @param[out] outrule
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_allocate_midorule(char *position, char *type, char *action, char *protocol, char *srcIAGuuid, char *src_port_min,  char *src_port_max, char *dstIAGuuid,
                           char *dst_port_min, char *dst_port_max, char *matchForwardFlow, char *matchReturnFlow, char *nat_target, char *nat_port_min, char *nat_port_max,
                           midorule *outrule) {
    int ret=0;
    if (!outrule) {
        return(1);
    }

    bzero(outrule, sizeof(midorule));

    if (position) snprintf(outrule->position, sizeof(outrule->position), "%s", position);
    if (type) snprintf(outrule->type, sizeof(outrule->type), "%s", type);
    if (action) snprintf(outrule->action, sizeof(outrule->action), "%s", action);
    if (protocol) snprintf(outrule->protocol, sizeof(outrule->protocol), "%s", protocol);
    if (srcIAGuuid) snprintf(outrule->srcIAGuuid, sizeof(outrule->srcIAGuuid), "%s", srcIAGuuid);
    if (src_port_min) snprintf(outrule->src_port_min, sizeof(outrule->src_port_min), "%s", src_port_min);
    if (src_port_max) snprintf(outrule->src_port_max, sizeof(outrule->src_port_max), "%s", src_port_max);
    if (dstIAGuuid) snprintf(outrule->dstIAGuuid, sizeof(outrule->dstIAGuuid), "%s", dstIAGuuid);
    if (dst_port_min) snprintf(outrule->dst_port_min, sizeof(outrule->dst_port_min), "%s", dst_port_min);
    if (dst_port_max) snprintf(outrule->dst_port_max, sizeof(outrule->dst_port_max), "%s", dst_port_max);
    if (matchForwardFlow) snprintf(outrule->matchForwardFlow, sizeof(outrule->matchForwardFlow), "%s", matchForwardFlow);
    if (matchReturnFlow) snprintf(outrule->matchReturnFlow, sizeof(outrule->matchReturnFlow), "%s", matchReturnFlow);
    if (nat_target) snprintf(outrule->nat_target, sizeof(outrule->nat_target), "%s", nat_target);
    if (nat_port_min) snprintf(outrule->nat_port_min, sizeof(outrule->nat_port_min), "%s", nat_port_min);
    if (nat_port_max) snprintf(outrule->nat_port_max, sizeof(outrule->nat_port_max), "%s", nat_port_max);

    return(ret);
}
*/

//!
//! Searches a list of chain rules in the argument for a rule matching the fields
//! specified in the variable argument section.
//!
//! @param[in]  rules pointer to a list of midoname structures containing chain rules.
//! @param[in]  max_rules number of chain rules in the list.
//! @param[out] outrule copy of the matching rule, if found. The init field is cleared if not found.
//! @param[in]  ... variable argument section
//!
//! @return 0 if the search is successful. 1 otherwise.
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_find_rule_from_list(midoname **rules, int max_rules, midoname *outrule, ...)
{
    int rc = 0, ret = 0, found = 0, i = 0;
    midoname myname;
    va_list ap = {{0}}, ap1 = {{0}};

    if (!outrule) {
        LOGWARN("Invalid argument: NULL pointer\n");
        return (1);
    }
    va_start(ap, outrule);

    bzero(&myname, sizeof(midoname));
    myname.tenant = strdup(VPCMIDO_TENANT);
    myname.resource_type = strdup("rules");
    myname.content_type = strdup("Rule");
    myname.vers = strdup("v2");

    found = 0;
    for (i = 0; i < max_rules && !found; i++) {
        if (!rules[i] || !rules[i]->init) {
            continue;
        }
        va_copy(ap1, ap);
        rc = mido_cmp_midoname_to_input_json_v(rules[i], &ap1);
        va_end(ap1);
        if (!rc) {
            mido_copy_midoname(outrule, rules[i]);
            found = 1;
        }
    }
    if (!found) {
        outrule->init = 0;
    }

    mido_free_midoname(&myname);
    va_end(ap);
    return (ret);
}

//!
//!
//!
//! @param[in]  chain
//! @param[out] outname
//! @param[in]  ... variable argument section
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_create_rule(midoname * chain, midoname * outname, midoname **memorules, int max_memorules, int * next_position, ...)
{
    int rc = 0, ret = 0, max_rules = 0, found = 0, i = 0;
    midoname myname;
    midoname *midorules = NULL;
    midoname **pmidorules = NULL;
    midoname **rules = NULL;
    va_list ap = {{0}}, ap1 = {{0}}, ap2 = {{0}};
    boolean gotrules = FALSE;

    va_start(ap, next_position);
    //va_copy(ap1, ap);
    va_copy(ap2, ap);

    bzero(&myname, sizeof(midoname));

    myname.tenant = strdup(chain->tenant);
    myname.resource_type = strdup("rules");
    myname.content_type = strdup("Rule");
    myname.vers = strdup("v2");

    if ((midonet_api_dirty_cache == 0) && (memorules != NULL)) {
        LOGTRACE("Checking %d rules in memory.\n", max_memorules);
        rules = memorules;
        max_rules = max_memorules;
        rc = 0;
    } else {
        if (midonet_api_dirty_cache == 1) {
            rc = mido_get_rules(chain, &midorules, &max_rules);
            if (max_rules > 0) {
                pmidorules = EUCA_ZALLOC(max_rules, sizeof (midoname **));
                for (int j = 0; j < max_rules; j++) {
                    pmidorules[j] = &(midorules[j]);
                }
            }
            gotrules = TRUE;
            rules = pmidorules;
        }
    }    

    if (!rc) {
        found = 0;
        for (i = 0; i < max_rules && !found; i++) {
            if (rules[i]->init == 0) {
                continue;
            }
            va_copy(ap1, ap);
            rc = mido_cmp_midoname_to_input_json_v(rules[i], &ap1);
            va_end(ap1);
            if (!rc) {
                if ((outname) && (outname != rules[i])) {
                    mido_copy_midoname(outname, rules[i]);
                }
                found = 1;
                LOGTRACE("FOUND RULE: %s\n", rules[i]->jsonbuf);
            }
        }
    }
    
    if (gotrules) {
        mido_free_midoname_list(midorules, max_rules);
        EUCA_FREE(midorules);
        if (pmidorules) {
            EUCA_FREE(pmidorules);
        }
    }

    LOGTRACE("MAX_RULE %s: %d\n", chain->name, max_rules);
    if (next_position) *next_position = max_rules + 1;

    if (!found) {
        rc = mido_create_resource_v(chain, 1, &myname, outname, &ap2);
        va_end(ap2);
        if (rc) {
            ret = 1;
        } else {
            if (next_position) *next_position = max_rules + 2;
        }
    }

    mido_free_midoname(&myname);
    va_end(ap);
    return (ret);
}

//!
//!
//!
//! @param[in] name
//! @param[in] ... variable argument section
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_update_rule(midoname * name, ...)
{
    return (0);
}

//!
//!
//!
//! @param[in] name
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_print_rule(midoname * name)
{
    return (mido_print_resource("ports", name));
}

//!
//!
//!
//! @param[in] name
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_delete_rule(midoname * name)
{
    return (mido_delete_resource(NULL, name));
}

//!
//!
//!
//! @param[in] devname
//! @param[in] port_type
//! @param[in] ip
//! @param[in] nw
//! @param[in] slashnet
//! @param[in] mac
//! @param[in] outname
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_create_port(midoname * devname, char *port_type, char *ip, char *nw, char *slashnet, char *mac, midoname * outname)
{
    int rc;
    midoname myname;

    bzero(&myname, sizeof(midoname));

    myname.tenant = strdup(devname->tenant);
    myname.resource_type = strdup("ports");
    myname.content_type = strdup("Port");
    myname.vers = strdup("v2");

    if (ip && nw && slashnet) {
        if (mac) {
            rc = mido_create_resource(devname, 1, &myname, outname, "type", port_type, "portAddress", ip, "networkAddress", nw, "networkLength", slashnet, "portMac", mac, NULL);
        } else {
            rc = mido_create_resource(devname, 1, &myname, outname, "type", port_type, "portAddress", ip, "networkAddress", nw, "networkLength", slashnet, NULL);
        }
    } else {
        rc = mido_create_resource(devname, 1, &myname, outname, "type", port_type, NULL);
    }

    mido_free_midoname(&myname);
    return (rc);
}

/*
//!
//!
//!
//! @param[in] name
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_read_port(midoname * name)
{
    return (mido_read_resource("ports", name, "application/vnd.org.midonet.Port-v2+json"));
}
*/

//!
//!
//!
//! @param[in] name
//! @param[in] ... variable argument section
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_update_port(midoname * name, ...)
{
    va_list al = { {0} };
    int ret = 0;
    va_start(al, name);
    ret = mido_update_resource("ports", "Port", "v2", name, &al);
    va_end(al);

    return (ret);
}

//!
//!
//!
//! @param[in] name
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_print_port(midoname * name)
{
    return (mido_print_resource("ports", name));
}

//!
//!
//!
//! @param[in] name
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_delete_port(midoname * name)
{
    return (mido_delete_resource(NULL, name));
}

//!
//!
//!
//! @param[in]  devname
//! @param[out] outnames
//! @param[out] outnames_max
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_get_ports(midoname * devname, midoname ** outnames, int *outnames_max)
{
    if (devname) {
        return (mido_get_resources(devname, 1, devname->tenant, "ports", "application/vnd.org.midonet.collection.Port-v2+json", outnames, outnames_max));
    } else {
        return (mido_get_resources(NULL, 0, VPCMIDO_TENANT, "ports", "application/vnd.org.midonet.collection.Port-v2+json", outnames, outnames_max));
    }
}

//!
//!
//!
//! @param[in]  chainname
//! @param[out] outnames
//! @param[out] outnames_max
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_get_rules(midoname * chainname, midoname ** outnames, int *outnames_max)
{
    return (mido_get_resources(chainname, 1, chainname->tenant, "rules", "application/vnd.org.midonet.collection.Rule-v2+json", outnames, outnames_max));
}

//!
//!
//!
//! @param[in] host
//! @param[in] port
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_unlink_host_port(midoname * host, midoname * port)
{
    int rc = 0, ret = 0;
    char url[EUCA_MAX_PATH];
    
    if (!host || !port) {
        LOGWARN("Invalid argument: NULL host or port - cannot unlink host-port.\n");
        return (1);
    }

    snprintf(url, EUCA_MAX_PATH, "http://localhost:8080/midonet-api/hosts/%s/ports/%s", host->uuid, port->uuid);
    rc = midonet_http_delete(url);
    if (rc) {
        ret = 1;
        LOGERROR("Failed to unlink %s %s\n", host->name, port->name);
    }
    return (ret);
}

//!
//!
//!
//! @param[in] host
//! @param[in] interface
//! @param[in] device
//! @param[in] port
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_link_host_port(midoname * host, char *interface, midoname * device, midoname * port)
{

    int rc = 0, ret = 0, found = 0;
    midoname myname;
    char *hinterface = NULL;

    bzero(&myname, sizeof(midoname));

    myname.tenant = strdup(device->tenant);
    myname.name = strdup("port");
    myname.resource_type = strdup("ports");
    myname.content_type = NULL;

    // check to see if the port is already mapped
    rc = mido_getel_midoname(port, "hostInterfacePort", &hinterface);
    if (!rc) {
        LOGDEBUG("Port %s already mapped to interface %s\n", port->name, hinterface);
        found = 1;
    }
    EUCA_FREE(hinterface);

    if (!found) {
        rc = mido_create_resource(host, 1, &myname, NULL, "bridgeId", device->uuid, "portId", port->uuid, "hostId", host->uuid, "interfaceName", interface, NULL);
        if (rc) {
            LOGWARN("Failed to link port %s to device %s\n", interface, device->name);
            ret = 1;
        }
    }

    mido_free_midoname(&myname);

    return (ret);
}

//!
//!
//!
//! @param[in] a
//! @param[in] b
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_link_ports(midoname * a, midoname * b)
{
    int rc = 0, found = 0, ret = 0;
    midoname myname;
    char *asideval = NULL, *bsideval = NULL;
    bzero(&myname, sizeof(midoname));

    myname.tenant = strdup(a->tenant);
    myname.name = strdup("link");
    myname.resource_type = strdup("link");
    myname.content_type = NULL;

    // check to see if link already exists before making new link
    if (!mido_getel_midoname(a, "peer", &asideval) && !mido_getel_midoname(b, "peer", &bsideval)) {
        if (strstr(asideval, b->uuid) && strstr(bsideval, a->uuid)) {
            LOGTRACE("ALREADY EXISTS: link from port %s to port %s\n", SP(a->uuid), SP(b->uuid));
            found = 1;
        }
    }
    EUCA_FREE(asideval);
    EUCA_FREE(bsideval);

    if (!found) {
        rc = mido_create_resource(a, 1, &myname, NULL, "peerId", b->uuid, NULL);
        if (rc) {
            ret = 1;
        }
    }

    mido_free_midoname(&myname);

    return (ret);
}

//!
//!
//!
//! @param[in] resource_type
//! @param[in] content_type
//! @param[in] name
//! @param[in] al
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_update_resource(char *resource_type, char *content_type, char *vers, midoname * name, va_list * al)
{
    int rc = 0, ret = 0;
    char *key = NULL, *val = NULL, *payload=NULL;
    struct json_object *jobj = NULL, *el = NULL;
    va_list ala = { {0} }, alb = { {0} };

    // check to see if resource needs updating
    va_copy(alb, *al);
    rc = mido_cmp_midoname_to_input_json_v(name, &alb);
    LOGDEBUG("update_resource cmp return: %d\n", rc);
    va_end(alb);

    if (!rc) {
        LOGDEBUG("resource to update matches in place resource - skipping update\n");
        return(ret);
    }

    va_copy(ala, *al);
    payload = mido_jsonize(name->tenant, &ala);
    va_end(ala);
    
    
    if (payload) {
        rc = midonet_http_put(name->uri, content_type, vers, payload);
        if (rc) {
            ret = 1;
        }
        EUCA_FREE(payload);
    }
    return(ret);
    
    
    jobj = json_tokener_parse(name->jsonbuf);
    //jobj = json_tokener_parse(payload);
    if (jobj) {
        key = va_arg(*al, char *);
        if (key)
            val = va_arg(*al, char *);
        while (key && val) {
            //            el = json_object_object_get(jobj, key);
            json_object_object_get_ex(jobj, key, &el);
            if (el) {
                json_object_object_add(jobj, key, json_object_new_string(val));
                //                json_object_put(el);
            } else {
                json_object_object_add(jobj, key, json_object_new_string(val));
            }
            key = va_arg(*al, char *);
            if (key)
                val = va_arg(*al, char *);
        }

        EUCA_FREE(name->jsonbuf);
        name->jsonbuf = strdup(json_object_to_json_string(jobj));

        json_object_put(jobj);
        ret = mido_update_midoname(name);
    } else {
        LOGERROR("ERROR: json_tokener_parse(...): returned NULL\n");
        ret = 1;
    }


    // ready to send the http_put
    if (!ret) {
        rc = midonet_http_put(name->uri, content_type, vers, name->jsonbuf);
        if (rc) {
            ret = 1;
        }
    }
    EUCA_FREE(payload);
    return (ret);
}

/*
//!
//!
//!
//! @param[in] resource_type
//! @param[in] name
//! @param[in] apistr
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_read_resource(char *resource_type, midoname * name, char *apistr)
{
    char url[EUCA_MAX_PATH], *outhttp = NULL;
    int rc = 0, ret = 0;

    snprintf(url, EUCA_MAX_PATH, "http://localhost:8080/midonet-api/%s/%s", resource_type, name->uuid);
    rc = midonet_http_get(url, apistr, &outhttp);
    if (rc) {
        ret = 1;
    } else {
        EUCA_FREE(name->jsonbuf);
        name->jsonbuf = strdup(outhttp);
        ret = mido_update_midoname(name);
    }
    EUCA_FREE(outhttp);
    return (ret);
}
*/

//!
//!
//!
//! @param[in] resource_type
//! @param[in] name
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_print_resource(char *resource_type, midoname * name)
{
    int ret = 0;
    struct json_object *jobj = NULL;

    jobj = json_tokener_parse(name->jsonbuf);
    if (!jobj) {
        LOGERROR("ERROR: json_tokener_parse(...): returned NULL\n");
        ret = 1;
    } else {
        LOGDEBUG("TYPE: %s NAME: %s UUID: %s\n", resource_type, SP(name->name), name->uuid);
        json_object_object_foreach(jobj, key, val) {
            LOGDEBUG("\t%s: %s\n", key, SP(json_object_get_string(val)));
        }
        json_object_put(jobj);
    }

    return (ret);
}

//!
//!
//!
//! @param[in] tenant
//! @param[in] al
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
char *mido_jsonize(char *tenant, va_list * al)
{
    char *payload = NULL;
    struct json_object *jobj = NULL, *jobj_sublist = NULL, *jarr_sublist = NULL;
    char *key = NULL, *val = NULL, *listobjtag = NULL, *listarrtag = NULL, *listjsontag = NULL;
    int listobjtag_count = 0, listarrtag_count = 0, listjsontag_count = 0;

    jobj = json_object_new_object();
    if (!jobj) {
        LOGERROR("ERROR: json_object_new_object(...): returned NULL\n");
        payload = NULL;
    } else {
        if (tenant) {
            json_object_object_add(jobj, "tenantId", json_object_new_string(tenant));
        }
        key = va_arg(*al, char *);
        if (key)
            val = va_arg(*al, char *);
        while (key && val) {
            //            LOGTRACE("HERE: %s/%s\n", key, val);
            if (!strcmp(val, "UNSET")) {
            } else {
                if (!strcmp(val, "jsonlist")) {
                    EUCA_FREE(listobjtag);
                    EUCA_FREE(listarrtag);
                    EUCA_FREE(listjsontag);
                    listobjtag = strdup(key);
                    listobjtag_count = 0;
                    jobj_sublist = json_object_new_object();
                } else if (!strcmp(val, "jsonjson")) {
                    EUCA_FREE(listobjtag);
                    EUCA_FREE(listarrtag);
                    EUCA_FREE(listjsontag);
                    listjsontag = strdup(key);
                    listjsontag_count = 0;
                    jobj_sublist = json_object_new_object();
                } else if (!strcmp(val, "jsonarr")) {
                    EUCA_FREE(listobjtag);
                    EUCA_FREE(listarrtag);
                    EUCA_FREE(listjsontag);
                    listarrtag = strdup(key);
                    listarrtag_count = 0;
                    jobj_sublist = json_object_new_array();
                } else if ((listobjtag && strstr(key, listobjtag)) || (listjsontag && strstr(key, listjsontag))) {
                    char *subkey = NULL;
                    subkey = strchr(key, ':');
                    subkey++;
                    if (!strcmp(val, "END")) {
                        // add the thing
                        if (listjsontag) {
                            if (listjsontag_count) {
                                json_object_object_add(jobj, listjsontag, jobj_sublist);
                            }
                            EUCA_FREE(listjsontag);
                            listjsontag = NULL;
                        } else if (listobjtag) {
                            if (listobjtag_count) {
                                jarr_sublist = json_object_new_array();
                                json_object_array_add(jarr_sublist, jobj_sublist);
                                json_object_object_add(jobj, listobjtag, jarr_sublist);
                            }
                            EUCA_FREE(listobjtag);
                            listobjtag = NULL;
                        }
                    } else {
                        listjsontag_count++;
                        listobjtag_count++;
                        json_object_object_add(jobj_sublist, subkey, json_object_new_string(val));
                    }
                } else if (listarrtag && strstr(key, listarrtag)) {
                    if (!strcmp(val, "END")) {
                        if (listarrtag_count) {
                            json_object_object_add(jobj, listarrtag, jobj_sublist);
                        }
                        EUCA_FREE(listarrtag);
                        listarrtag = NULL;
                    } else {
                        listarrtag_count++;
                        json_object_array_add(jobj_sublist, json_object_new_string(val));
                    }
                } else {
                    if (listobjtag) {
                        jarr_sublist = json_object_new_array();
                        json_object_array_add(jarr_sublist, jobj_sublist);
                        json_object_object_add(jobj, listobjtag, jarr_sublist);
                        EUCA_FREE(listobjtag);
                        listobjtag = NULL;
                    } else if (listjsontag) {
                        json_object_object_add(jobj, listjsontag, jobj_sublist);
                        EUCA_FREE(listjsontag);
                        listjsontag = NULL;
                    } else if (listarrtag) {
                        json_object_object_add(jobj, listarrtag, jobj_sublist);
                        EUCA_FREE(listarrtag);
                        listarrtag = NULL;
                    }
                    json_object_object_add(jobj, key, json_object_new_string(val));
                }
            }
            key = va_arg(*al, char *);
            if (key)
                val = va_arg(*al, char *);
        }
        if (listobjtag) {
            jarr_sublist = json_object_new_array();
            json_object_array_add(jarr_sublist, jobj_sublist);
            json_object_object_add(jobj, listobjtag, jarr_sublist);
            EUCA_FREE(listobjtag);
            listobjtag = NULL;
        } else if (listjsontag) {
            json_object_object_add(jobj, listjsontag, jobj_sublist);
            EUCA_FREE(listjsontag);
            listjsontag = NULL;
        } else if (listarrtag) {
            json_object_object_add(jobj, listarrtag, jobj_sublist);
            EUCA_FREE(listarrtag);
            listarrtag = NULL;
        }
        //        printf("JSON: %s\n", json_object_to_json_string(jobj));
        payload = strdup(json_object_to_json_string(jobj));
        //        LOGTRACE("PAYLOAD: %s\n", payload);
        json_object_put(jobj);
    }
    return (payload);
}

//!
//!
//!
//! @param[in]  parents
//! @param[in]  max_parents
//! @param[in]  newname
//! @param[out] outname
//! @param[in]  ... variable argument part
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_create_resource(midoname * parents, int max_parents, midoname * newname, midoname * outname, ...)
{
    int ret = 0;
    va_list al;
    va_start(al, outname);
    ret = mido_create_resource_v(parents, max_parents, newname, outname, &al);
    va_end(al);
    return (ret);
}

//!
//!
//!
//! @param[in]  parents
//! @param[in]  max_parents
//! @param[in]  newname
//! @param[out] outname
//! @param[in]  al
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_create_resource_v(midoname * parents, int max_parents, midoname * newname, midoname * outname, va_list * al)
{
    int ret = 0, rc = 0;
    char url[EUCA_MAX_PATH];
    char *outloc = NULL, *outhttp = NULL, *payload = NULL;
    char tmpbuf[EUCA_MAX_PATH];
    int i;


    if (outname) {
        if (outname->init) {
            LOGTRACE("ALREADY EXISTS (INSTALLED): %s/%s/%s\n", outname->resource_type, outname->uuid, outname->jsonbuf);
            
            rc = mido_cmp_midoname_to_input_json_v(outname, al);
            if (rc) {
                LOGTRACE("ALREADY EXISTS BUT WILL UPDATE\n");
                mido_update_resource(newname->resource_type, newname->content_type, newname->vers, outname, al);
            } else {
                LOGTRACE("ALREADY EXISTS BUT SKIP UPDATE\n");
            }

            LOGTRACE("ALREADY EXISTS (NEW): %d/%s\n", rc, newname->resource_type);
            
            return (0);
        }
        bzero(outname, sizeof(midoname));
    }

    //  construct the payload
    payload = mido_jsonize(newname->tenant, al);

    if (payload) {
        if (!parents) {
            snprintf(url, EUCA_MAX_PATH, "http://localhost:8080/midonet-api/%s", newname->resource_type);
        } else {
            snprintf(url, EUCA_MAX_PATH, "http://localhost:8080/midonet-api/");
            for (i = 0; i < max_parents; i++) {
                tmpbuf[0] = '\0';
                snprintf(tmpbuf, EUCA_MAX_PATH, "%s/%s/", parents[i].resource_type, parents[i].uuid);
                strcat(url, tmpbuf);
            }
            tmpbuf[0] = '\0';
            snprintf(tmpbuf, EUCA_MAX_PATH, "%s", newname->resource_type);
            strcat(url, tmpbuf);
        }

        // perform the create
        rc = midonet_http_post(url, newname->content_type, newname->vers, payload, &outloc);
        if (rc) {
            LOGERROR("midonet_http_post(%s, ...) failed\n", url);
            ret = 1;
        }
    } else {
        LOGERROR("could not generate payload\n");
        ret = 1;
    }

    // if all goes well, store the new resource
    if (!ret) {
        if (outname && outloc) {
            rc = midonet_http_get(outloc, NULL, &outhttp);
            if (rc) {
                LOGWARN("Failed to retrieve new resource from %s\n", outloc);
                ret = 1;
            } else {
                mido_copy_midoname(outname, newname);
                if (outhttp) {
                    outname->jsonbuf = strdup(outhttp);
                }

                /*
                if (newname->tenant)
                    outname->tenant = strdup(newname->tenant);
                if (outhttp)
                    outname->jsonbuf = strdup(outhttp);
                if (newname->resource_type)
                    outname->resource_type = strdup(newname->resource_type);
                if (newname->content_type)
                    outname->content_type = strdup(newname->content_type);
                if (newname->vers)
                    outname->vers = strdup(newname->vers);
                if (newname->uri)
                    outname->uri = strdup(newname->uri);
                */

                outname->init = 1;
                ret = mido_update_midoname(outname);
            }
        }
    }

    EUCA_FREE(payload);
    EUCA_FREE(outhttp);
    EUCA_FREE(outloc);
    return (ret);

}

int mido_cmp_midoname(midoname *a, midoname *b) {
    int ret=0;

    if (!a || !b) {
        return(1);
    }

    if (!a->init || !b->init) {
        return(1);
    }

    
    ret = strcmp(a->tenant, b->tenant);
    if (!ret) ret = strcmp(a->name, b->name);
    if (!ret) ret = strcmp(a->uuid, b->uuid);
    if (!ret) ret = strcmp(a->jsonbuf, b->jsonbuf);
    if (!ret) ret = strcmp(a->resource_type, b->resource_type);
    if (!ret) ret = strcmp(a->content_type, b->content_type);
    if (!ret) ret = strcmp(a->vers, b->vers);
    if (!ret) ret = strcmp(a->uri, b->uri);
    
    if (ret) {
        return(1);
    }
    return(0);
}

//!
//!
//!
//! @param[in] dst
//! @param[in] src
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
void mido_copy_midoname(midoname * dst, midoname * src)
{
    if (!dst || !src) {
        return;
    }
    if (dst == src) {
        LOGINFO("mido_copy_midoname: %s %s dst == src skipping\n", dst->resource_type, dst->name);
        return;
    }
    if (dst->init) {
        mido_free_midoname(dst);
    }

    bzero(dst, sizeof(midoname));
    if (src->tenant)
        dst->tenant = strdup(src->tenant);
    if (src->name)
        dst->name = strdup(src->name);
    if (src->uuid)
        dst->uuid = strdup(src->uuid);
    if (src->jsonbuf)
        dst->jsonbuf = strdup(src->jsonbuf);
    if (src->resource_type)
        dst->resource_type = strdup(src->resource_type);
    if (src->content_type)
        dst->content_type = strdup(src->content_type);
    if (src->vers)
        dst->vers = strdup(src->vers);
    if (src->uri)
        dst->uri = strdup(src->uri);

    dst->init = src->init;
}

//!
//!
//!
//! @param[in]  tenant
//! @param[in]  name
//! @param[in]  uuid
//! @param[in]  resource_type
//! @param[in]  content_type
//! @param[in]  jsonbuf
//! @param[out] outname
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_create_midoname(char *tenant, char *name, char *uuid, char *resource_type, char *content_type, char *vers, char *uri, char *jsonbuf, midoname * outname)
{
    if (!outname) {
        return (1);
    }

    bzero(outname, sizeof(midoname));
    if (tenant)
        outname->tenant = strdup(tenant);
    if (name)
        outname->name = strdup(name);
    if (uuid)
        outname->uuid = strdup(uuid);
    if (resource_type)
        outname->resource_type = strdup(resource_type);
    if (content_type)
        outname->content_type = strdup(content_type);
    if (jsonbuf)
        outname->jsonbuf = strdup(jsonbuf);
    if (vers)
        outname->vers = strdup(vers);
    if (uri)
        outname->uri = strdup(uri);

    outname->init = 1;

    return (0);
}

//!
//!
//!
//! @param[in] name
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_update_midoname(midoname * name)
{
    int ret = 0;
    struct json_object *jobj = NULL, *el = NULL;
    char special_uuid[EUCA_MAX_PATH];

    if (!name || (!name->init)) {
        return (1);
    }
    jobj = json_tokener_parse(name->jsonbuf);
    if (!jobj) {
        printf("ERROR: json_tokener_parse(...): returned NULL\n");
        ret = 1;
    } else {
        //        el = json_object_object_get(jobj, "id");
        json_object_object_get_ex(jobj, "id", &el);
        if (el) {
            EUCA_FREE(name->uuid);
            name->uuid = strdup(json_object_get_string(el));
            //            json_object_put(el);
        }

        //        el = json_object_object_get(jobj, "tenantId");
        json_object_object_get_ex(jobj, "tenantId", &el);
        if (el) {
            EUCA_FREE(name->tenant);
            name->tenant = strdup(json_object_get_string(el));
            //            json_object_put(el);
        }

        //el = json_object_object_get(jobj, "name");
        json_object_object_get_ex(jobj, "name", &el);
        if (el) {
            EUCA_FREE(name->name);
            name->name = strdup(json_object_get_string(el));
            //            json_object_put(el);
        }

        //        el = json_object_object_get(jobj, "uri");
        json_object_object_get_ex(jobj, "uri", &el);
        if (el) {
            EUCA_FREE(name->uri);
            name->uri = strdup(json_object_get_string(el));
            //            json_object_put(el);
        }

        // special cases
        if (!strcmp(name->resource_type, "dhcp")) {
            char *subnet = NULL, *slashnet = NULL;
            EUCA_FREE(name->uuid);
            EUCA_FREE(name->name);

            //el = json_object_object_get(jobj, "subnetPrefix");
            json_object_object_get_ex(jobj, "subnetPrefix", &el);
            if (el) {
                subnet = strdup(json_object_get_string(el));
                //                json_object_put(el);
            }

            //el = json_object_object_get(jobj, "subnetLength");
            json_object_object_get_ex(jobj, "subnetLength", &el);
            if (el) {
                slashnet = strdup(json_object_get_string(el));
                //                json_object_put(el);
            }

            if (subnet && slashnet) {
                snprintf(special_uuid, EUCA_MAX_PATH, "%s_%s", subnet, slashnet);
                name->uuid = strdup(special_uuid);
            }
            EUCA_FREE(subnet);
            EUCA_FREE(slashnet);

        } else if (!strcmp(name->resource_type, "ip_addrs")) {
            char *ip = NULL;
            EUCA_FREE(name->uuid);
            EUCA_FREE(name->name);
            //            el = json_object_object_get(jobj, "addr");
            json_object_object_get_ex(jobj, "addr", &el);
            if (el) {
                ip = strdup(json_object_get_string(el));
                //                json_object_put(el);
            }
            
            if (ip) {
                snprintf(special_uuid, EUCA_MAX_PATH, "versions/6/ip_addrs/%s", ip);
                name->uuid = strdup(special_uuid);
            }
            EUCA_FREE(ip);

        } else {
            if (!name->uuid || !strlen(name->uuid)) {
                //                el = json_object_object_get(jobj, "uri");
                json_object_object_get_ex(jobj, "uri", &el);
                if (el) {
                    EUCA_FREE(name->uuid);
                    name->uuid = strdup(json_object_get_string(el));
                    //                    json_object_put(el);
                }
            }
        }

        json_object_put(jobj);
    }

    if (!name->name || (strlen(name->name) <= 0)) {
        name->name = strdup(name->uuid);
    }

    return (ret);
}

//!
//!
//!
//! @param[in] parentname
//! @param[in] name
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_delete_resource(midoname * parentname, midoname * name)
{
    int rc = 0, ret = 0;
    char url[EUCA_MAX_PATH];
    json_object *jobj = NULL, *el = NULL;

    if (!name || !name->init) {
        return (0);
    }

    url[0] = '\0';

    jobj = json_tokener_parse(name->jsonbuf);
    if (jobj) {
        //        el = json_object_object_get(jobj, "uri");
        json_object_object_get_ex(jobj, "uri", &el);
        if (el) {
            snprintf(url, EUCA_MAX_PATH, "%s", json_object_get_string(el));
            //            json_object_put(el);
        }
        json_object_put(jobj);
    }

    if (!strlen(url)) {
        if (parentname) {
            snprintf(url, EUCA_MAX_PATH, "http://localhost:8080/midonet-api/%s/%s/%s/%s", parentname->resource_type, parentname->uuid, name->resource_type, name->uuid);
        } else {
            snprintf(url, EUCA_MAX_PATH, "http://localhost:8080/midonet-api/%s/%s", name->resource_type, name->uuid);
        }
    }

    LOGTRACE("resource to delete: %s/%s url to delete: %s\n", SP(name->name), SP(name->uuid), url);

    rc = midonet_http_delete(url);
    if (rc) {
        ret = 1;
    }

    if (!ret) {
        mido_free_midoname(name);
    }
    return (ret);
}

//!
//!
//!
//! @param[in] contents
//! @param[in] size
//! @param[in] nmemb
//! @param[in] in_params
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
static size_t mem_writer(void *contents, size_t size, size_t nmemb, void *in_params)
{
    struct mem_params_t *params = (struct mem_params_t *)in_params;

    if (!params->mem) {
        params->mem = calloc(1, 1);
    }
    params->mem = realloc(params->mem, params->size + (size * nmemb) + 1);
    if (params->mem == NULL) {
        return (0);
    }
    memcpy(&(params->mem[params->size]), contents, size * nmemb);
    params->size += size * nmemb;
    params->mem[params->size] = '\0';

    return (size * nmemb);
}

//!
//!
//!
//! @param[in] contents
//! @param[in] size
//! @param[in] nmemb
//! @param[in] in_params
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
static size_t mem_reader(void *contents, size_t size, size_t nmemb, void *in_params)
{
    struct mem_params_t *params = (struct mem_params_t *)in_params;
    size_t bytes_to_copy = 0;

    if (!params->mem || params->size <= 0) {
        return (0);
    }

    if (!contents) {
        LOGERROR("ERROR: no mem to write into\n");
        params->size = 0;
        return (0);
    }

    bytes_to_copy = (params->size < (size * nmemb)) ? params->size : (size * nmemb);

    memcpy(contents, params->mem, bytes_to_copy);
    params->size -= bytes_to_copy;
    params->mem += bytes_to_copy;

    return (bytes_to_copy);
}

//!
//!
//!
//! @param[in]  url
//! @param[in]  apistr
//! @param[out] out_payload
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int midonet_http_get(char *url, char *apistr, char **out_payload)
{
    CURL *curl = NULL;
    CURLcode curlret;
    struct mem_params_t mem_writer_params = { 0, 0 };
    int ret = 0;
    long httpcode = 0L;
    struct curl_slist *headers = NULL;
    char hbuf[EUCA_MAX_PATH];
    long long timer=0;

    timer = time_usec();
    if (!strstr(url, "system_state")) {
        //mido_check_state();
    }
    *out_payload = NULL;

    curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, mem_writer);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&mem_writer_params);

    if (apistr && strlen(apistr)) {
        snprintf(hbuf, EUCA_MAX_PATH, "accept: %s", apistr);
        headers = curl_slist_append(headers, hbuf);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }

    curlret = curl_easy_perform(curl);
    if (curlret != CURLE_OK) {
        LOGERROR("ERROR: curl_easy_perform(): %s\n", curl_easy_strerror(curlret));
        ret = 1;
    }
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpcode);
    if (httpcode != 200L) {
        LOGWARN("curl get http code: %ld\nurl: %s\napistr: %s\n", httpcode, url, apistr);
        ret = 1;
    }
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    curl_global_cleanup();

    // convert to payload out

    if (!ret) {
        if (mem_writer_params.mem && mem_writer_params.size > 0) {
            *out_payload = calloc(mem_writer_params.size + 1, sizeof(char));
            memcpy(*out_payload, mem_writer_params.mem, mem_writer_params.size + 1);
        } else {
            LOGERROR("ERROR: no data to return after successful curl operation\n");
            ret = 1;
        }
    }
    if (mem_writer_params.mem)
        free(mem_writer_params.mem);

    LOGTRACE("total time for http operation: %f seconds\n", (time_usec() - timer) / 100000.0);
    http_gets++;
    return (ret);
}

//!
//!
//!
//! @param[in] url
//! @param[in] resource_type
//! @param[in] payload
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int midonet_http_put(char *url, char *resource_type, char *vers, char *payload)
{
    CURL *curl = NULL;
    CURLcode curlret;
    struct mem_params_t mem_reader_params = { 0, 0 };
    char hbuf[EUCA_MAX_PATH];
    struct curl_slist *headers = NULL;
    int ret = 0;
    long httpcode = 0L;
    long long timer=0;

    timer = time_usec();

    mido_check_state();

    mem_reader_params.mem = payload;
    mem_reader_params.size = strlen(payload) + 1;

    curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(curl, CURLOPT_PUT, 1L);
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, mem_reader);
    curl_easy_setopt(curl, CURLOPT_READDATA, (void *)&mem_reader_params);
    curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (long)mem_reader_params.size);

    if (resource_type && vers) {
        snprintf(hbuf, EUCA_MAX_PATH, "Content-Type: application/vnd.org.midonet.%s-%s+json", resource_type, vers);
    } else if (resource_type) {
        snprintf(hbuf, EUCA_MAX_PATH, "Content-Type: application/vnd.org.midonet.%s-%s+json", resource_type, "v1");
    }
    headers = curl_slist_append(headers, hbuf);
    snprintf(hbuf, EUCA_MAX_PATH, "Expect:");
    headers = curl_slist_append(headers, hbuf);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    LOGDEBUG("PUT PAYLOAD: %s\n", SP(payload));
    curlret = curl_easy_perform(curl);
    if (curlret != CURLE_OK) {
        LOGERROR("ERROR: curl_easy_perform(): %s\n", curl_easy_strerror(curlret));
        ret = 1;
    }
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpcode);
    if (httpcode == 200L || httpcode == 204L) {
        ret = 0;
        midonet_api_dirty_cache = 1;
    } else {
        LOGWARN("curl put http code: %ld\n", httpcode);
        LOGINFO("\turl %s payload %s\n", url, SP(payload));
        ret = 1;
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    curl_global_cleanup();

    LOGTRACE("total time for http operation: %f seconds\n", (time_usec() - timer) / 100000.0);
    http_puts++;
    return (ret);
}

//!
//!
//!
//! @param[in] content
//! @param[in] size
//! @param[in] nmemb
//! @param[in] params
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
static size_t header_find_location(char *content, size_t size, size_t nmemb, void *params)
{
    char *buf = NULL;
    char **loc = (char **)params;

    buf = calloc((size * nmemb) + 1, sizeof(char));
    memcpy(buf, content, size * nmemb);
    buf[size * nmemb] = '\0';

    if (buf && strstr(buf, "Location: ")) {
        *loc = calloc(strlen(buf), sizeof(char));
        sscanf(buf, "Location: %s", *loc);
    }
    free(buf);

    return (size * nmemb);
}

//!
//!
//!
//! @param[in]  url
//! @param[in]  resource_type
//! @param[in]  payload
//! @param[out] out_payload
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int midonet_http_post(char *url, char *resource_type, char *vers, char *payload, char **out_payload)
{
    CURL *curl = NULL;
    CURLcode curlret;
    int ret = 0;
    char *loc = NULL, hbuf[EUCA_MAX_PATH];
    struct curl_slist *headers = NULL;
    long long timer=0;
    long httpcode = 0L;

    timer = time_usec();

    mido_check_state();

    *out_payload = NULL;

    curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, url);
    //    curl_easy_setopt(curl, CURLOPT_HEADER, 1L);
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(payload));
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_find_location);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &loc);
    if (!resource_type || strlen(resource_type) <= 0) {
        snprintf(hbuf, EUCA_MAX_PATH, "Content-Type: application/json");
    } else {
        if (resource_type && vers) {
            snprintf(hbuf, EUCA_MAX_PATH, "Content-Type: application/vnd.org.midonet.%s-%s+json", resource_type, vers);
        } else if (resource_type) {
            snprintf(hbuf, EUCA_MAX_PATH, "Content-Type: application/vnd.org.midonet.%s-%s+json", resource_type, "v1");
        }
    }
    headers = curl_slist_append(headers, hbuf);
    //    headers = curl_slist_append(headers, "Content-Type: application/vnd.org.midonet");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    LOGDEBUG("POST PAYLOAD: %s\n", SP(payload));
    curlret = curl_easy_perform(curl);
    if (curlret != CURLE_OK) {
        LOGERROR("ERROR: curl_easy_perform(): %s\n", curl_easy_strerror(curlret));
        ret = 1;
    } else {
        midonet_api_dirty_cache = 1;
    }
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpcode);
    if ((httpcode != 200L) && (httpcode != 201L)) {
        LOGWARN("curl post http code: %ld\n", httpcode);
        LOGINFO("\turl %s payload %s\n", url, payload);
        ret = 1;
    }
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    curl_global_cleanup();

    if (!ret) {
        if (loc) {
            *out_payload = strdup(loc);
        }
    }
    EUCA_FREE(loc);

    LOGTRACE("total time for http operation: %f seconds\n", (time_usec() - timer) / 100000.0);
    http_posts++;
    return (ret);
}

//!
//!
//!
//! @param[in] url
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int midonet_http_delete(char *url)
{
    CURL *curl = NULL;
    CURLcode curlret;
    int ret = 0;
    long long timer=0;
    long httpcode = 0L;

    timer = time_usec();

    mido_check_state();
        
    curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");

    LOGDEBUG("DELETE PAYLOAD: %s\n", SP(url));
    curlret = curl_easy_perform(curl);
    if (curlret != CURLE_OK) {
        LOGERROR("ERROR: curl_easy_perform(): %s\n", curl_easy_strerror(curlret));
        ret = 1;
    } else {
        midonet_api_dirty_cache = 1;
    }
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpcode);
    if ((httpcode != 200L) && (httpcode != 204L)) {
        LOGWARN("curl delete http code: %ld\n", httpcode);
        LOGINFO("\turl %s\n", SP(url));
        ret = 1;
    }

    curl_easy_cleanup(curl);
    curl_global_cleanup();

    LOGTRACE("total time for http operation: %f seconds\n", (time_usec() - timer) / 100000.0);
    http_deletes++;
    return (ret);
}

//!
//! Searches for a mido router route specified in the arguments from a list (also
//! specified in the arguments). 
//!
//! @param[in]  routes list of routes to look for a matching route entry.
//! @param[in]  max_routes number of routes in the list.
//! @param[in]  rport router port to be routed.
//! @param[in]  src source subnet.
//! @param[in]  src_slashnet source slash net.
//! @param[in]  dst destination subnet.
//! @param[in]  dst_slashnet destination slash net.
//! @param[in]  next_hop_ip next hop.
//! @param[in]  weight route weight.
//! @param[out] index at which the route was found. -1 if the route was not found.
//! @return 0 if the route was found. 1 otherwise.
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_find_route_from_list(midoname *routes, int max_routes, midoname *rport, char *src, char *src_slashnet, char *dst, char *dst_slashnet, char *next_hop_ip, char *weight, int *foundidx)
{
    int rc = 0, found = 0, ret = 0;
    midoname myname;
    int i = 0;

    bzero(&myname, sizeof(midoname));

    myname.tenant = strdup(VPCMIDO_TENANT);
    myname.resource_type = strdup("routes");
    myname.content_type = NULL;

    found = 0;
    for (i = 0; i < max_routes && !found; i++) {
        if (routes[i].init == 0) {
            continue;
        }
        if (strcmp(next_hop_ip, "UNSET")) {
            rc = mido_cmp_midoname_to_input(&(routes[i]), "srcNetworkAddr", src, "srcNetworkLength", src_slashnet, "dstNetworkAddr", dst, "dstNetworkLength", dst_slashnet,
                    "type", "Normal", "nextHopPort", rport->uuid, "weight", weight, "nextHopGateway", next_hop_ip, NULL);
            if (!rc) {
                found = 1;
            }
        } else {
            rc = mido_cmp_midoname_to_input(&(routes[i]), "srcNetworkAddr", src, "srcNetworkLength", src_slashnet, "dstNetworkAddr", dst, "dstNetworkLength", dst_slashnet,
                    "type", "Normal", "nextHopPort", rport->uuid, "weight", weight, NULL);
            if (!rc) {
                found = 1;
            }
        }
    }
    if (found) {
        if (foundidx) {
            *foundidx = i;
        }
        ret = 0;
    } else {
        ret = 1;
    }
    mido_free_midoname(&myname);
    return (ret);
}

//!
//! Creates a new router route as specified in the argument. 
//!
//! @param[in]  mido data structure holding all discovered MidoNet resources.
//! @param[in]  router router of interest.
//! @param[in]  rport router port to be routed.
//! @param[in]  src source subnet.
//! @param[in]  src_slashnet source slash net.
//! @param[in]  dst destination subnet.
//! @param[in]  dst_slashnet destination slash net.
//! @param[in]  next_hop_ip next hop.
//! @param[in]  weight route weight.
//! @param[out] outname newly created route information.
//! @return 0 if the route is successfully created. 1 otherwise.
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_create_route(mido_config *mido, midoname *router, midoname *rport, char *src, char *src_slashnet, char *dst, char *dst_slashnet, char *next_hop_ip, char *weight, midoname * outname)
{
    int rc = 0, found = 0, ret = 0;
    midoname myname;
    midoname *routes = NULL;
    int max_routes = 0;

    bzero(&myname, sizeof(midoname));

    myname.tenant = strdup(router->tenant);
    myname.resource_type = strdup("routes");
    myname.content_type = NULL;

    // only create the route if it doesn't already exist
    mido_resource_router *crouter = find_mido_router(mido, router->name);
    if (crouter == NULL) {
        // Likely a newly created router
        routes = NULL;
        max_routes = 0;
    } else {
        routes = crouter->routes;
        max_routes = crouter->max_routes;
    }

    rc = mido_find_route_from_list(routes, max_routes, rport, src, src_slashnet, dst, dst_slashnet, next_hop_ip, weight, NULL);
    if (rc == 0) {
        found = 1;
    }
    // route doesn't already exist, create it
    if (!found) {
        // delete old resource, if present
        if (outname && outname->init) {
            mido_delete_route(outname);
        }

        if (strcmp(next_hop_ip, "UNSET")) {
            rc = mido_create_resource(router, 1, &myname, outname, "srcNetworkAddr", src, "srcNetworkLength", src_slashnet, "dstNetworkAddr", dst, "dstNetworkLength", dst_slashnet,
                                      "type", "Normal", "nextHopPort", rport->uuid, "weight", weight, "nextHopGateway", next_hop_ip, NULL);
        } else {
            rc = mido_create_resource(router, 1, &myname, outname, "srcNetworkAddr", src, "srcNetworkLength", src_slashnet, "dstNetworkAddr", dst, "dstNetworkLength", dst_slashnet,
                                      "type", "Normal", "nextHopPort", rport->uuid, "weight", weight, NULL);
        }
        if (rc) {
            ret = 1;
        }
    }

    mido_free_midoname(&myname);
    return (ret);
}

//!
//!
//!
//! @param[in] name
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_delete_route(midoname * name)
{
    return (mido_delete_resource(NULL, name));
}

//!
//!
//!
//! @param[in]  router
//! @param[out] outnames
//! @param[out] outnames_max
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_get_routes(midoname * router, midoname ** outnames, int *outnames_max)
{
    return (mido_get_resources(router, 1, router->tenant, "routes", "application/vnd.org.midonet.collection.Route-v1+json", outnames, outnames_max));
}

//!
//!
//!
//! @param[in] one
//! @param[in] two
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int json_object_cmp(json_object * one, json_object * two)
{
    int onetype = 0, twotype = 0, onesubtype = 0, twosubtype = 0, rc = 0, ret = 0, i = 0;
    char *oneel = NULL, *twoel = NULL;
    json_object *twoval = NULL;

    if (!one && !two) {
        LOGTRACE("both are null\n");
        return (0);
    } else if ((one && !two) || (!one && two)) {
        LOGTRACE("one is null\n");
        return (1);
    }

    LOGTRACE("one=%s\n", json_object_to_json_string(one));
    LOGTRACE("two=%s\n", json_object_to_json_string(two));
    onetype = json_object_get_type(one);
    twotype = json_object_get_type(two);
    if (onetype != twotype) {
        LOGTRACE("types differ\n");
        return(1);
    }

    if (onetype == json_type_object) {
        json_object_object_foreach(one, onekey, oneval) {
            LOGTRACE("evaluating key %s\n", onekey);
            onesubtype = json_object_get_type(oneval);
            //            twoval = json_object_object_get(two, onekey);
            json_object_object_get_ex(two, onekey, &twoval);
            twosubtype = json_object_get_type(twoval);
            if (onesubtype == json_type_object) {
                // recurse
                rc = json_object_cmp(oneval, twoval);
            } else if (onesubtype == json_type_array) {
                for (i = 0; i < json_object_array_length(oneval) && !rc; i++) {
                    rc = json_object_cmp(json_object_array_get_idx(oneval, i), json_object_array_get_idx(twoval, i));
                }
            } else {
                oneel = strdup(SP(json_object_get_string(oneval)));
                twoel = strdup(SP(json_object_get_string(twoval)));
                LOGTRACE("strcmp: %s/%s\n", oneel, twoel);
                rc = strcmp(oneel, twoel);
                EUCA_FREE(oneel);
                EUCA_FREE(twoel);
                if (rc != 0) {
                    ret = 1;
                    break;
                }
            }
            //            json_object_put(twoval);
            if (rc) {
                ret = 1;
            }
        }
    } else {
        LOGTRACE("comparing strings: one=%s two=%s\n", json_object_to_json_string(one), json_object_to_json_string(two));
        ret = strcmp(json_object_to_json_string(one), json_object_to_json_string(two));
    }

    LOGTRACE("result of cmp: %d\n", ret);
    return (ret);
}

//!
//!
//!
//! @param[in] name
//! @param[in] ... variable argument part
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_cmp_midoname_to_input_json(midoname * name, ...)
{
    va_list al = { {0} };
    int ret = 0;

    va_start(al, name);
    ret = mido_cmp_midoname_to_input_json_v(name, &al);
    va_end(al);
    return (ret);
}

//!
//!
//!
//! @param[in] name
//! @param[in] al
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_cmp_midoname_to_input_json_v(midoname * name, va_list * al)
{
    va_list ala = { {0} };
    char *jsonbuf = NULL;
    json_object *srcjobj = NULL, *dstjobj = NULL;
    int ret = 0;

    va_copy(ala, *al);
    jsonbuf = mido_jsonize(NULL, &ala);
    va_end(ala);

    LOGTRACE("new=%s\n", SP(jsonbuf));
    LOGTRACE("old=%s\n", SP(name->jsonbuf));

    if (jsonbuf && name->jsonbuf) {

        dstjobj = json_tokener_parse(name->jsonbuf);
        srcjobj = json_tokener_parse(jsonbuf);

        // special case el removal
        if (!strcmp(name->resource_type, "rules")) {
            // for chain rules, remove the position element
            json_object_object_del(srcjobj, "position");
            json_object_object_del(dstjobj, "position");
        }

        if (json_object_cmp(srcjobj, dstjobj)) {
            ret = 1;
        } else {
            ret = 0;
        }

    } else {
        ret = 1;
    }

    if (srcjobj)
        json_object_put(srcjobj);
    if (dstjobj)
        json_object_put(dstjobj);

    //    LOGTRACE("RETURNING %d\n", ret);
    EUCA_FREE(jsonbuf);
    return (ret);
}

//!
//!
//!
//! @param[in] name
//! @param[in] ... variable argument
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_cmp_midoname_to_input(midoname * name, ...)
{
    va_list al = { {0} };
    int rc = 0;
    char *key = NULL, *dstval = NULL, *srcval = NULL;

    va_start(al, name);

    key = va_arg(al, char *);
    if (key)
        dstval = va_arg(al, char *);
    while (key && dstval) {
        rc = mido_getel_midoname(name, key, &srcval);
        if (!rc) {
            if (strcmp(dstval, srcval)) {
                EUCA_FREE(srcval);
                va_end(al);
                return (1);
            }
            EUCA_FREE(srcval);
        } else if (rc && !strcmp(dstval, "UNSET")) {
            // skip
        } else {
            EUCA_FREE(srcval);
            va_end(al);
            return (1);
        }
        key = va_arg(al, char *);
        if (key)
            dstval = va_arg(al, char *);
    }

    va_end(al);
    LOGTRACE("RESOURCE ALREADY IN PLACE: %s\n", SP(name->uuid));
    return (0);
}

//!
//!
//!
//! @param[in]  tenant
//! @param[out] outnames
//! @param[out] outnames_max
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_get_routers(char *tenant, midoname ** outnames, int *outnames_max)
{
    return (mido_get_resources(NULL, 0, tenant, "routers", "application/vnd.org.midonet.collection.Router-v2+json", outnames, outnames_max));
}

//!
//!
//!
//! @param[in]  tenant
//! @param[out] outnames
//! @param[out] outnames_max
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_get_bridges(char *tenant, midoname ** outnames, int *outnames_max)
{
    return (mido_get_resources(NULL, 0, tenant, "bridges", "application/vnd.org.midonet.collection.Bridge-v2+json", outnames, outnames_max));
}

//!
//!
//!
//! @param[in]  tenant
//! @param[out] outnames
//! @param[out] outnames_max
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_get_chains(char *tenant, midoname ** outnames, int *outnames_max)
{
    return (mido_get_resources(NULL, 0, tenant, "chains", "application/vnd.org.midonet.collection.Chain-v1+json", outnames, outnames_max));
}

//!
//!
//!
//! @param[in]  parents
//! @param[in]  max_parents
//! @param[in]  tenant
//! @param[in]  resource_type
//! @param[in]  apistr
//! @param[out] outnames
//! @param[out] outnames_max
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_get_resources(midoname * parents, int max_parents, char *tenant, char *resource_type, char *apistr, midoname ** outnames, int *outnames_max)
{
    int rc = 0, ret = 0, i = 0;
    char *payload = NULL, url[EUCA_MAX_PATH], tmpbuf[EUCA_MAX_PATH];
    midoname *names = NULL;
    int names_max = 0;

    *outnames = NULL;
    *outnames_max = 0;

    bzero(url, EUCA_MAX_PATH);
    if (!parents) {
        snprintf(url, EUCA_MAX_PATH, "http://localhost:8080/midonet-api/%s?tenant_id=%s", resource_type, tenant);
    } else {
        snprintf(url, EUCA_MAX_PATH, "http://localhost:8080/midonet-api/");
        for (i = 0; i < max_parents; i++) {
            bzero(tmpbuf, EUCA_MAX_PATH);
            snprintf(tmpbuf, EUCA_MAX_PATH, "%s/%s/", parents[i].resource_type, parents[i].uuid);
            strcat(url, tmpbuf);
        }
        bzero(tmpbuf, EUCA_MAX_PATH);
        snprintf(tmpbuf, EUCA_MAX_PATH, "%s?tenant_id=%s", resource_type, tenant);
        strcat(url, tmpbuf);
    }


    rc = midonet_http_get(url, apistr, &payload);
    if (!rc) {
        struct json_object *jobj = NULL, *resource = NULL;

        jobj = json_tokener_parse(payload);
        if (!jobj) {
            LOGWARN("cannot tokenize midonet response: check midonet health\n");
        } else {
            //            jobj = json_object_get(jobj);
            if (json_object_is_type(jobj, json_type_array)) {

                names_max = 0;
                names = calloc(json_object_array_length(jobj), sizeof(midoname));

                for (i = 0; i < json_object_array_length(jobj); i++) {

                    resource = json_object_array_get_idx(jobj, i);
                    if (resource) {

                        /*
                           json_object_object_foreach(resource, key, val) {
                           printf("\t%s: %s\n", key, SP(json_object_get_string(val)));
                           }
                         */

                        names[names_max].tenant = strdup(tenant);
                        names[names_max].jsonbuf = strdup(json_object_to_json_string(resource));
                        names[names_max].resource_type = strdup(resource_type);
                        names[names_max].content_type = NULL;
                        names[names_max].init = 1;
                        mido_update_midoname(&(names[names_max]));
                        names_max++;
                        //                        json_object_put(resource);
                    }
                }
            }
            json_object_put(jobj);
        }
        EUCA_FREE(payload);
    }

    if (names && (names_max > 0)) {
        *outnames = calloc(names_max, sizeof(midoname));
        memcpy(*outnames, names, sizeof(midoname) * names_max);
        *outnames_max = names_max;
    }
    //    mido_free_midoname_list(names, names_max);
    EUCA_FREE(names);

    return (ret);
}

//!
//!
//!
//! @param[out] outnames
//! @param[out] outnames_max
//!
//! @return
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int mido_get_hosts(midoname ** outnames, int *outnames_max)
{
    int rc = 0, ret = 0, i = 0, hostup = 0;
    char *payload = NULL, url[EUCA_MAX_PATH];
    midoname *names = NULL;
    int names_max = 0;

    *outnames = NULL;
    *outnames_max = 0;

    bzero(url, EUCA_MAX_PATH);
    snprintf(url, EUCA_MAX_PATH, "http://localhost:8080/midonet-api/hosts");
    rc = midonet_http_get(url, "application/vnd.org.midonet.collection.Host-v2+json", &payload);
    if (!rc) {
        struct json_object *jobj = NULL, *host = NULL, *el = NULL;

        jobj = json_tokener_parse(payload);
        if (!jobj) {
            LOGWARN("cannot tokenize midonet response: check midonet health\n");
        } else {
            if (json_object_is_type(jobj, json_type_array)) {
                //                printf("HMM: %s, %d\n", json_object_to_json_string(jobj), json_object_array_length(jobj));
                names_max = 0;
                names = calloc(json_object_array_length(jobj), sizeof(midoname));
                for (i = 0; i < json_object_array_length(jobj); i++) {
                    //                    bzero(&((*outnames)[i]), sizeof(midoname));
                    host = json_object_array_get_idx(jobj, i);
                    if (host) {
                        /*
                           json_object_object_foreach(host, key, val) {
                           printf("\t%s: %s\n", key, SP(json_object_get_string(val)));
                           }
                         */

                        //el = json_object_object_get(host, "alive");
                        json_object_object_get_ex(host, "alive", &el);
                        if (el) {
                            if (!strcmp(json_object_get_string(el), "false")) {
                                // host is down, skip
                                hostup = 0;
                            } else {
                                hostup = 1;
                            }
                            //                            json_object_put(el);
                        }

                        if (hostup) {
                            names[names_max].jsonbuf = strdup(json_object_to_json_string(host));

                            //el = json_object_object_get(host, "id");
                            json_object_object_get_ex(host, "id", &el);
                            if (el) {
                                names[names_max].uuid = strdup(json_object_get_string(el));
                                //                                json_object_put(el);
                            }

                            //el = json_object_object_get(host, "name");
                            json_object_object_get_ex(host, "name", &el);
                            if (el) {
                                names[names_max].name = strdup(json_object_get_string(el));
                                //                                json_object_put(el);
                            }

                            names[names_max].resource_type = strdup("hosts");
                            names[names_max].content_type = NULL;
                            names[names_max].init = 1;
                            names_max++;
                        }
                        //                        json_object_put(host);
                    }
                }
            }
            json_object_put(jobj);
        }

        EUCA_FREE(payload);
    }

    if (names && (names_max > 0)) {
        *outnames = calloc(names_max, sizeof(midoname));
        memcpy(*outnames, names, sizeof(midoname) * names_max);
        *outnames_max = names_max;
    }
    //    mido_free_midoname_list(names, names_max);
    EUCA_FREE(names);

    return (ret);
}

//!
//! Retrieves interfaces of the specified host as detected by midolman.
//!
//! @param[in]  host host of interest.
//! @param[in]  iftype interface type to search and return.
//! @param[out] outnames array of interfaces of the host.
//! @param[out] outnames_max number of interfaces.
//!
//! @return 0 on success. 1 otherwise.
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note memory allocated for outnames must be released by the caller. The caller
//!       is also required to release prior memory allocations to outnames before
//!       calling this function.
//!
int mido_get_interfaces(midoname *host, u32 iftype, u32 ifendpoint, midoname **outnames, int *outnames_max)
{
    int rc = 0, ret = 0, i = 0, getif = 0;
    char *payload = NULL, url[EUCA_MAX_PATH];
    midoname *names = NULL;
    int names_max = 0;

    if ((host == NULL) || (host->init == 0) || (host->uuid == NULL)) {
        LOGWARN("Invalid argument: invalid MidoNet host.\n");
        return (1);
    }
    *outnames = NULL;
    *outnames_max = 0;

    bzero(url, EUCA_MAX_PATH);
    snprintf(url, EUCA_MAX_PATH, "http://localhost:8080/midonet-api/hosts/%s/interfaces", host->uuid);
    rc = midonet_http_get(url, "application/vnd.org.midonet.collection.Interface-v1+json", &payload);
    if (!rc) {
        struct json_object *jobj = NULL, *interface = NULL, *el = NULL;

        jobj = json_tokener_parse(payload);
        if (!jobj) {
            LOGWARN("cannot tokenize midonet response: check midonet health\n");
        } else {
            if (json_object_is_type(jobj, json_type_array)) {
                names_max = 0;
                names = EUCA_ZALLOC(json_object_array_length(jobj), sizeof (midoname));
                for (i = 0; i < json_object_array_length(jobj); i++) {
                    interface = json_object_array_get_idx(jobj, i);
                    if (interface) {
                        if (iftype == MIDO_HOST_INTERFACE_ALL) {
                            getif = 1;
                        } else {
                            getif = 0;
                        }
                        json_object_object_get_ex(interface, "type", &el);
                        if (el) {
                            const char *tmp = NULL;
                            tmp = json_object_get_string(el);
                            if ((!getif) && (iftype & MIDO_HOST_INTERFACE_PHYSICAL) && (!strcmp(tmp, "Physical"))) {
                                getif = 1;
                            }
                            if ((!getif) && (iftype & MIDO_HOST_INTERFACE_VIRTUAL) && (!strcmp(tmp, "Virtual"))) {
                                getif = 1;
                            }
                            if ((!getif) && (iftype & MIDO_HOST_INTERFACE_TUNNEL) && (!strcmp(tmp, "Tunnel"))) {
                                getif = 1;
                            }
                            if ((!getif) && (iftype & MIDO_HOST_INTERFACE_UNKNOWN) && (!strcmp(tmp, "Unknown"))) {
                                getif = 1;
                            }
                        }
                        if (getif) {
                            if (ifendpoint == MIDO_HOST_INTERFACE_ENDPOINT_ALL) {
                                getif = 1;
                            } else {
                                getif = 0;
                            }
                            json_object_object_get_ex(interface, "endpoint", &el);
                            if (el) {
                                const char *tmp = json_object_get_string(el);
                                if ((!getif) && (ifendpoint & MIDO_HOST_INTERFACE_ENDPOINT_PHYSICAL) && (!strcmp(tmp, "PHYSICAL"))) {
                                    getif = 1;
                                }
                                if ((!getif) && (ifendpoint & MIDO_HOST_INTERFACE_ENDPOINT_DATAPAH) && (!strcmp(tmp, "DATAPATH"))) {
                                    getif = 1;
                                }
                                if ((!getif) && (ifendpoint & MIDO_HOST_INTERFACE_ENDPOINT_LOCALHOST) && (!strcmp(tmp, "LOCALHOST"))) {
                                    getif = 1;
                                }
                                if ((!getif) && (ifendpoint & MIDO_HOST_INTERFACE_ENDPOINT_UNKNOWN) && (!strcmp(tmp, "UNKNOWN"))) {
                                    getif = 1;
                                }
                            }
                        }

                        if (getif) {
                            names[names_max].jsonbuf = strdup(json_object_to_json_string(interface));
                            json_object_object_get_ex(interface, "id", &el);
                            if (el) {
                                names[names_max].uuid = strdup(json_object_get_string(el));
                            }
                            json_object_object_get_ex(interface, "name", &el);
                            if (el) {
                                names[names_max].name = strdup(json_object_get_string(el));
                            }
                            names[names_max].resource_type = strdup("interfaces");
                            names[names_max].content_type = NULL;
                            names[names_max].init = 1;
                            names_max++;
                        }
                    }
                }
            }
            json_object_put(jobj);
        }
        EUCA_FREE(payload);
    }

    if (names && (names_max > 0)) {
        *outnames = EUCA_ZALLOC(names_max, sizeof(midoname));
        memcpy(*outnames, names, sizeof(midoname) * names_max);
        *outnames_max = names_max;
    }
    EUCA_FREE(names);

    return (ret);
}

//!
//! Retrieves the IPv4 addresses of the specified host as detected by midolman.
//!
//! @param[in]  host host of interest.
//! @param[out] outnames array of IPv4 addresses of the host.
//! @param[out] outnames_max number of addresses.
//!
//! @return 0 on success. 1 otherwise.
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note memory allocated for outnames must be released by the caller. The caller
//!       is also required to release prior memory allocations to outnames before
//!       calling this function.
//!
int mido_get_addresses(midoname *host, u32 **outnames, int *outnames_max)
{
    int rc = 0, ret = 0;
    int i = 0;
    midoname *names = NULL;
    int names_max = 0;
    char **addrs;
    int max_addrs;
    u32 *hips = NULL;

    if ((host == NULL) || (host->init == 0) || (host->uuid == NULL)) {
        LOGWARN("Invalid argument: invalid MidoNet host.\n");
        return (1);
    }
    LOGTRACE("retrieving IPv4 addresses of %s\n", host->name);
    rc = mido_get_interfaces(host, MIDO_HOST_INTERFACE_ALL, (MIDO_HOST_INTERFACE_ENDPOINT_PHYSICAL | MIDO_HOST_INTERFACE_ENDPOINT_UNKNOWN), &names, &names_max);

    if ((rc == 0) && (names != NULL) && (names_max > 0)) {
        *outnames_max = 0;
        for (i = 0; i < names_max; i++) {
            rc = mido_getarr_midoname(&(names[i]), "addresses", &addrs, &max_addrs);
            LOGTRACE("%s %s - max_addrs: %d\n", host->name, names[i].name, max_addrs);
            if ((rc == 0) && (max_addrs > 0)) {
                hips = EUCA_REALLOC(hips, *outnames_max + max_addrs, sizeof (u32));
                if (hips == NULL) {
                    LOGFATAL("out of memory - onmax %d, maxa %d.\n", *outnames_max, max_addrs);
                    return (1);
                }
                bzero(&(hips[*outnames_max]), max_addrs * sizeof (u32));
                for (int j = 0; j < max_addrs; j++) {
                    if (strlen(addrs[j]) > 16) {
                        LOGTRACE("\tskipping %s - not an IPv4 address.\n", addrs[j]);
                    } else {
                        hips[*outnames_max] = dot2hex(addrs[j]);
                        if ((hips[*outnames_max] & 0xa9fe0000) == 0xa9fe0000) {
                            LOGTRACE("\tskipping %s - link local address\n", addrs[j]);
                            hips[*outnames_max] = 0;
                        } else {
                            LOGTRACE("\tFound %s\n", addrs[j]);
                            (*outnames_max)++;
                        }
                    }
                    EUCA_FREE(addrs[j]);
                }
                EUCA_FREE(addrs);
            }
        }
        *outnames = hips;
    }

    mido_free_midoname_list(names, names_max);
    EUCA_FREE(names);

    return (ret);
}

int mido_check_state(void) {
    int ret = 0, rc = 0, mido = 0, retries = 300, i;
    char url[EUCA_MAX_PATH];
    char *outbuf = NULL;
    
    bzero(url, EUCA_MAX_PATH);
    snprintf(url, EUCA_MAX_PATH, "http://localhost:8080/midonet-api/system_state");

    for (i=0; i<retries && !mido; i++) {
        rc = midonet_http_get(url, "application/vnd.org.midonet.SystemState-v2+json", &outbuf);
        if (rc) {
            LOGERROR("midonet-api is not available (attempt %d/%d): check midonet-api health to resume eucanetd operation\n", i, retries);
            sleep(1);
        } else {
            LOGDEBUG("midonet-api system state: %s\n", SP(outbuf));
            mido = 1;
        }
        EUCA_FREE(outbuf);    
    }
    if (!mido) {
        LOGFATAL("midonet-api is not reachable after %d retries: eucanetd shutting down\n", retries);
        ret = 1;
        exit(1);
    }

    return(ret);
}

//!
//! Creates an empty midoname_list structure.
//!
//! @return pointer to the newly created midoname_list structure or NULL in case of failure.
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
midoname_list *midoname_list_new(void) {
    midoname_list *res = NULL;

    LOGTRACE("Creating a new midoname_list.\n");
    res = EUCA_ZALLOC(1, sizeof (midoname_list));
    if (res == NULL) {
        LOGFATAL("out of memory - allocating midoname_list.\n");
        return NULL;
    }
    res->capacity = MIDONAME_LIST_DEFAULT_CAPACITY;
    res->mnames = EUCA_ZALLOC(res->capacity, sizeof (midoname));
    if (res->mnames == NULL) {
        LOGFATAL("out of memory - allocating midoname buffer for midoname_list.\n");
        EUCA_FREE(res);
        return NULL;
    }
    return (res);
}

//!
//! Releases resources used by the midoname_list structure in the argument.
//!
//! @param[in]  midoname_list structure of interest.
//! @return 0 on success. 1 otherwise.
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note The list is assumed to be created using midoname_list_new()
//!
int midoname_list_free(midoname_list *list) {
    LOGTRACE("Releasing midoname_list at %p\n", list);
    if (list == NULL) {
        return (0);
    }
    mido_free_midoname_list(list->mnames, list->size);
    EUCA_FREE(list->mnames);
    bzero(list, sizeof (midoname_list));
    EUCA_FREE(list);
    return (0);
}

//!
//! Returns a free midoname element from the midoname_list in the argument.
//!
//! @param[in]  midoname_list structure of interest.
//! @return pointer to an unused midoname structure. NULL on any failure.
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note The list is assumed to be created using midoname_list_new()
//!
midoname *midoname_list_get_midoname(midoname_list *list) {
    midoname *res = NULL;
    LOGTRACE("Retrieving an unused midoname element.\n");
    if (list->size == list->capacity) {
        list->mnames = EUCA_REALLOC(list->mnames, list->capacity + MIDONAME_LIST_DEFAULT_CAPACITY, sizeof (midoname));
        bzero(&(list->mnames[list->capacity]), MIDONAME_LIST_DEFAULT_CAPACITY * sizeof (midoname));
        list->capacity += MIDONAME_LIST_DEFAULT_CAPACITY;
        if (list->mnames == NULL) {
            LOGFATAL("out of memory - (re)allocating midoname buffer for midoname_list.\n ");
            return NULL;
        }
    }
    res = &(list->mnames[list->size]);
    (list->size)++;
    return res;
}

//!
//! Comparator function for mido_iphostmap_entry structures.
//!
//! @param[in] p1 pointer to mido_iphostmap_entry 1.
//! @param[in] p2 pointer to mido_iphostmap_entry 1.
//!
//! @return 0 iff p1->ip == p2->ip. -1 iff p1->ip < p2->ip. 1 iff p1->ip > p2->ip.
//!
//! @see
//!
//! @pre
//!
//! @post
//!
//! @note
//!
int compare_iphostmap_entry(const void *p1, const void *p2) {
    mido_iphostmap_entry *e1 = (mido_iphostmap_entry *) p1;
    mido_iphostmap_entry *e2 = (mido_iphostmap_entry *) p2;

    if (e1->ip == e2->ip) {
        return 0;
    }
    if (e1->ip < e2->ip) {
        return -1;
    } else {
        return 1;
    }
}

#ifdef MIDONET_API_TEST
//!
//!
//!
//! @param[in] argc
//! @param[in] argv
//!
//! @return
//!
//! @note
//!
int main(int argc, char **argv)
{
    exit(0);
}
#endif
