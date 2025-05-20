#include "imagetool.h"
#include  "mkimage.h"
#include <image.h>

struct ipod_header {
    char checksum[4];
    char magic[4];
};

static struct ipod_header ipodimage_header;

static int ipodimage_check_params(struct image_tool_params *params)
{
    if (!params)
        return 0;

    return 0;
}

static int ipodimage_verify_header(unsigned char *ptr, int image_size,
        struct image_tool_params *params)
{
    if (image_size < sizeof(struct ipod_header))
        return -1;

    int i;
    uint8_t* data = (uint8_t*)ptr;
    uint32_t checksum = data[3] | (data[2] << 8) | (data[1] << 16) | (data[0] << 24);
    uint32_t platform = 0x67326e6e;  // nn2g
    checksum -= 62;
    if (*((uint32_t*)&data[4]) != platform) return -2;
    for (i = 0; i < image_size - 8; i++)
    {
        checksum -= data[i + 8];
    }
    if (checksum) return -1;
    return 0;
}

static void ipodimage_print_header(const void *ptr)
{
    struct ipod_header *hdr = (struct ipod_header *)ptr;
    printf("Image Type    : Apple/Samsung S5L ipod image\n");
}

static void ipodimage_set_header(void *ptr, struct stat *sbuf, int ifd,
        struct image_tool_params *params)
{
    struct ipod_header *hdr = (struct ipod_header *)ptr;
    uint32_t platform = 0x67326e6e;  // nn2g
    memcpy(&hdr->magic, &platform, 4);

    uint32_t checksum = 62;
    uint8_t* data = (uint8_t*)ptr;
    for (int i = 0; i < sbuf->st_size - 8; i++)
    {
        checksum += data[i + 8];
    }
    checksum = cpu_to_be32(checksum);
    memcpy(&hdr->checksum, &checksum, 4);
}

static int ipodimage_check_image_types(uint8_t type)
{
	if (type == IH_TYPE_IPODIMAGE)
		return EXIT_SUCCESS;
	return EXIT_FAILURE;
}

U_BOOT_IMAGE_TYPE(
    ipodimage,
    "ipod image support",
    sizeof(struct ipod_header),
    (void *)&ipodimage_header,
    ipodimage_check_params,
    ipodimage_verify_header,
    ipodimage_print_header,
    ipodimage_set_header,
    NULL,
    ipodimage_check_image_types,
    NULL,
    NULL
);
