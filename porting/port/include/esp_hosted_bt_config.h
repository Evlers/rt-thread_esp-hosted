/*
 * Copyright (c) 2025 Evlers
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Change Logs:
 * Date         Author      Notes
 * 2024-12-26   Evlers      first implementation
 */

#ifndef __ESP_HOSTED_BT_CONFIG_H__
#define __ESP_HOSTED_BT_CONFIG_H__

// Hosted BT defines
#if CONFIG_ESP_ENABLE_BT
#define H_BT_ENABLED 1
#else
#define H_BT_ENABLED 0
#endif

#if CONFIG_BT_NIMBLE_ENABLED
#define H_BT_HOST_ESP_NIMBLE 1
#else
#define H_BT_HOST_ESP_NIMBLE 0
#endif

#if CONFIG_ESP_HCI_VHCI
#define H_BT_USE_VHCI 1
#else
#define H_BT_USE_VHCI 0
#endif

#define H_BT_ENABLE_LL_INIT 0

#endif
