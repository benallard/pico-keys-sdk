/*
 * This file is part of the Pico HSM SDK distribution (https://github.com/polhenarejos/pico-hsm-sdk).
 * Copyright (c) 2022 Pol Henarejos.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "apdu.h"
#include "hsm.h"
#include "usb.h"
#include <stdio.h>

uint8_t *rdata_gr = NULL;
uint16_t rdata_bk = 0x0;
extern uint32_t timeout;

int process_apdu() {
    led_set_blink(BLINK_PROCESSING);
    if (!current_app)
    {
        if (INS(apdu) == 0xA4 && P1(apdu) == 0x04 && (P2(apdu) == 0x00 || P2(apdu) == 0x4)) { //select by AID
            for (int a = 0; a < num_apps; a++) {
                if ((current_app = apps[a].select_aid(&apps[a]))) {
                    return set_res_sw(0x90,0x00);
                }
            }
        }
        return set_res_sw(0x6a, 0x82);
    }
    if (current_app->process_apdu)
        return current_app->process_apdu();
    return set_res_sw(0x6D, 0x00);
}

size_t apdu_process(const uint8_t *buffer, size_t buffer_size) {
    apdu.header = (uint8_t *)buffer;
    apdu.nc = apdu.ne = 0;
    if (buffer_size == 4) {
        apdu.nc = apdu.ne = 0;
        if (apdu.ne == 0)
            apdu.ne = 256;
    }
    else if (buffer_size == 5) {
        apdu.nc = 0;
        apdu.ne = apdu.header[4];
        if (apdu.ne == 0)
            apdu.ne = 256;
    }
    else if (apdu.header[4] == 0x0 && buffer_size >= 7) {
        if (buffer_size == 7) {
            apdu.ne = (apdu.header[5] << 8) | apdu.header[6];
            if (apdu.ne == 0)
                apdu.ne = 65536;
        }
        else {
            apdu.ne = 0;
            apdu.nc = (apdu.header[5] << 8) | apdu.header[6];
            apdu.data = apdu.header+7;
            if (apdu.nc+7+2 == buffer_size) {
                apdu.ne = (apdu.header[buffer_size-2] << 8) | apdu.header[buffer_size-1];
                if (apdu.ne == 0)
                    apdu.ne = 65536;
            }
        }
    }
    else {
        apdu.nc = apdu.header[4];
        apdu.data = apdu.header+5;
        apdu.ne = 0;
        if (apdu.nc+5+1 == buffer_size) {
            apdu.ne = apdu.header[buffer_size-1];
            if (apdu.ne == 0)
                apdu.ne = 256;
        }
    }
    //printf("apdu.nc %ld, apdu.ne %ld\r\n",apdu.nc,apdu.ne);
    if (apdu.header[1] == 0xc0) {
        //printf("apdu.ne %ld, apdu.rlen %d, bk %x\r\n",apdu.ne,apdu.rlen,rdata_bk);
        timeout_stop();
        *(uint16_t *)rdata_gr = rdata_bk;
        if (apdu.rlen <= apdu.ne) {
            driver_exec_finished_cont(apdu.rlen+2, rdata_gr-usb_get_tx());
        }
        else {
            rdata_gr += apdu.ne;
            rdata_bk = *rdata_gr;
            rdata_gr[0] = 0x61;
            if (apdu.rlen - apdu.ne >= 256)
                rdata_gr[1] = 0;
            else
                rdata_gr[1] = apdu.rlen - apdu.ne;
            driver_exec_finished_cont(apdu.ne+2, rdata_gr-apdu.ne-usb_get_tx());
            apdu.rlen -= apdu.ne;
        }
        //Prepare next RAPDU
        apdu.sw = 0;
        apdu.rlen = 0;
        usb_prepare_response();
        return 0;
    }
    else {
        apdu.sw = 0;
        apdu.rlen = 0;
        apdu.rdata = usb_prepare_response();
        rdata_gr = apdu.rdata;
        return 1;
    }
    return 0;
}

uint16_t set_res_sw(uint8_t sw1, uint8_t sw2) {
    apdu.sw = (sw1 << 8) | sw2;
    if (sw1 != 0x90)
        res_APDU_size = 0;
    return make_uint16_t(sw1, sw2);
}

void apdu_finish() {
    apdu.rdata[apdu.rlen] = apdu.sw >> 8;
    apdu.rdata[apdu.rlen+1] = apdu.sw & 0xff;
    timeout_stop();
    if ((apdu.rlen + 2 + 10) % 64 == 0)
    { // FIX for strange behaviour with PSCS and multiple of 64
        apdu.ne = apdu.rlen - 2;
    }
}

size_t apdu_next() {
    if (apdu.rlen <= apdu.ne)
        return apdu.rlen + 2;
    else {
        rdata_gr = apdu.rdata+apdu.ne;
        rdata_bk = *(uint16_t *)rdata_gr;
        rdata_gr[0] = 0x61;
        if (apdu.rlen - apdu.ne >= 256)
            rdata_gr[1] = 0;
        else
            rdata_gr[1] = apdu.rlen - apdu.ne;
        apdu.rlen -= apdu.ne;
    }
    return apdu.ne + 2;
}
