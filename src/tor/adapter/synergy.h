/* Copyright (c) 2014, Razor Developers */
/* See LICENSE for licensing information */

/**
 * \file synergy.h
 * \brief Headers for synergy.cpp
 **/

#ifndef TOR_RAZOR_H
#define TOR_RAZOR_H

#ifdef __cplusplus
extern "C" {
#endif

    char const* synergy_tor_data_directory(
    );

    char const* synergy_service_directory(
    );

    int check_interrupted(
    );

    void set_initialized(
    );

    void wait_initialized(
    );

#ifdef __cplusplus
}
#endif

#endif

