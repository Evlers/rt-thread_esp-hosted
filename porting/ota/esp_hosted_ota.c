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
 * 2025-01-13   Evlers      first implementation
 */

#include "rtthread.h"
#include "common.h"
#include "rpc_wrap.h"
#include "esp_hosted_ota.h"

#ifdef PKG_USING_WEBCLIENT
#include "webclient.h"
#endif

#define BLOCK_SIZE 			1400

static void print_progress (size_t cur_size, size_t total_size)
{
    static unsigned char progress_sign[50 + 1];
    static rt_tick_t last_tick;
    uint8_t per = (rt_uint64_t)cur_size * 100llu / (rt_uint64_t)total_size;
    rt_tick_t current_tick = rt_tick_get();
    static uint32_t data_rate, last_size;

    if (cur_size == 0)
    {
        data_rate = 0;
        last_tick = 0;
        last_size = 0;
    }

    rt_tick_t elapsed_time = current_tick - last_tick;

    if (elapsed_time >= rt_tick_from_millisecond(100) || cur_size == total_size)
    {
        uint32_t download_rate = (cur_size - last_size) * rt_tick_from_millisecond(1000) / elapsed_time;

        last_size = cur_size;
        last_tick = current_tick;
        data_rate = (data_rate * 3 + download_rate) / 4;

        if (per > 100)
        {
            per = 100;
        }

        for (int i = 0; i < 50; i++)
        {
            if (i < (per / 2))
            {
                progress_sign[i] = '=';
            }
            else if ((per / 2) == i)
            {
                progress_sign[i] = '>';
            }
            else
            {
                progress_sign[i] = ' ';
            }
        }

        progress_sign[sizeof(progress_sign) - 1] = '\0';
        rt_kprintf("\033[2K\rwriting: [%s] %d%%  %u | %d KB/s", progress_sign, per, cur_size, data_rate / 1024);
    }
}

#ifdef RT_USING_DFS
static esp_err_t esp_hosted_ota_local (char* image_path)
{
	FILE *f = NULL;
	size_t write_total = 0, read_len, file_size;
	char *block = rt_malloc(BLOCK_SIZE);

	if (block == NULL)
	{
		rt_kprintf("failed to allocate memory for ota block\n");
		return ESP_FAIL;
	}

	f = fopen(image_path, "rb");
	if (f == NULL)
	{
		rt_kprintf("failed to open file %s\n", image_path);
		goto __free;
	}

	if (fseek(f, 0, SEEK_END) != 0)
	{
		rt_kprintf("fseek failed\n");
		goto __free;
	}

	file_size = ftell(f);
	if (fseek(f, 0, SEEK_SET) != 0)
	{
		rt_kprintf("fseek failed\n");
		goto __free;
	}

	rt_kprintf("esp-hosted ota update started\n");
	rt_kprintf("erasing the ota partition..\n");

	if (rpc_ota_begin() != ESP_OK)
	{
		rt_kprintf("ota begin failed!!\n");
		goto __free;
	}

	print_progress(0, file_size);
	while (write_total < file_size)
	{
		read_len = min(BLOCK_SIZE, file_size - write_total);
		print_progress(write_total + read_len, file_size);
		if (fread(block, 1, read_len, f) != read_len)
		{
			rt_kprintf("fread failed\n");
			goto __error;
		}
		if (rpc_ota_write((uint8_t *)block, read_len) != ESP_OK)
		{
			rt_kprintf("\nfirmware write failed!!\n");
			goto __error;
		}
		write_total += read_len;
	}
	print_progress(write_total, file_size);
	rt_kprintf("\nfirmware write success!!\n");

	__error:
	if (rpc_ota_end() && write_total == file_size)
	{
		rt_kprintf("ota end failed!!\n");
	}
	else if (write_total == file_size)
	{
		rt_kprintf("slave will restart after 5 sec\n");
	}

	__free:
	if (f)
		fclose(f);
	rt_free(block);

	return ESP_OK;
}
#endif /* RT_USING_DFS */

#ifdef PKG_USING_WEBCLIENT
static esp_err_t esp_hosted_ota_http (char *url)
{
	int resp_status = 0;
	size_t write_total = 0, read_len;
	struct webclient_session* session = RT_NULL;
	char *block = rt_malloc(BLOCK_SIZE);

	if (block == NULL)
	{
		rt_kprintf("failed to allocate memory for ota block\n");
		return ESP_FAIL;
	}

    if ((session = webclient_session_create(WEBCLIENT_HEADER_BUFSZ)) == RT_NULL)
    {
        return ESP_FAIL;
    }

    if ((resp_status = webclient_get(session, url)) != 200)
    {
        rt_kprintf("get file failed, wrong response: %d (-0x%X).\n", resp_status, resp_status);
        goto __free;
    }

	rt_kprintf("esp-hosted ota update started\n");
	rt_kprintf("erasing the ota partition..\n");

	if (rpc_ota_begin() != ESP_OK)
	{
		rt_kprintf("ota begin failed!!\n");
		goto __free;
	}

	if (session->content_length < 0)
    {
        while (1)
        {
            read_len = webclient_read(session, block, BLOCK_SIZE);
            if (read_len > 0)
            {
                if (rpc_ota_write((uint8_t *)block, read_len) != ESP_OK)
				{
					rt_kprintf("\nfirmware write failed!!\n");
					goto __error;
				}
                write_total += read_len;
            }
            else
            {
                break;
            }
        }
    }
    else
    {
		print_progress(0, session->content_length);
		while (write_total < session->content_length)
        {
            read_len = webclient_read(session, block, min(BLOCK_SIZE, session->content_length - write_total));
            if (read_len > 0)
            {
				print_progress(write_total + read_len, session->content_length);
				if (rpc_ota_write((uint8_t *)block, read_len) != ESP_OK)
				{
					rt_kprintf("\nfirmware write failed!!\n");
					goto __error;
				}
                write_total += read_len;
            }
            else
            {
                break;
            }
        }
		print_progress(write_total, session->content_length);
    }
	rt_kprintf("\nfirmware write success!!\n");

	__error:
	if (rpc_ota_end() && write_total == session->content_length)
	{
		rt_kprintf("ota end failed!!\n");
	}
	else if (write_total == session->content_length || session->content_length < 0)
	{
		rt_kprintf("slave will restart after 5 sec\n");
	}
	else
	{
        rt_kprintf("data receiving incomplete, need to receive length: %u, actual receiving: %u\n", session->content_length, write_total);
	}

	__free:
	if (session != RT_NULL)
    {
        webclient_close(session);
        session = RT_NULL;
    }
	rt_free(block);
}
#endif /* PKG_USING_WEBCLIENT */

#ifdef RT_USING_FINSH
static int esp_ota (int argc, char **argv)
{
	if (argc < 2)
    {
        rt_kprintf("usage: esp_ota <file path / url>\n");
        return 0;
    }

	esp_hosted_ota(argv[1]);
	return 0;
}
MSH_CMD_EXPORT(esp_ota, esp_ota <file>);
#endif /* RT_USING_FINSH */

esp_err_t esp_hosted_ota (char *url_or_path)
{
	if (url_or_path == NULL)
	{
		rt_kprintf("url_or_path is NULL\n");
		return ESP_FAIL;
	}

#ifdef PKG_USING_WEBCLIENT
	if (strstr(url_or_path, "http://") != NULL || strstr(url_or_path, "https://") != NULL)
	{
		return esp_hosted_ota_http(url_or_path);
	}
#endif

#ifdef RT_USING_DFS
	return esp_hosted_ota_local(url_or_path);
#else
	rt_kprintf("plase enable dfs\n");
	return ESP_FAIL;
#endif
}
