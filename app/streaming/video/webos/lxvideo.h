#include <gst/gst.h>

typedef enum lxdebufext_hdr_type {
    LXDEBUFEXT_HDR_TYPE_SDR=0x0,
    LXDEBUFEXT_HDR_TYPE_HDR10=0x1,
    LXDEBUFEXT_HDR_TYPE_HLG=0x2,
    LXDEBUFEXT_HDR_TYPE_PRIME=0x3,
    LXDEBUFEXT_HDR_TYPE_DOLBY=0x4,
    LXDEBUFEXT_HDR_TYPE_MAX=0x5,
    LXDEBUFEXT_HDR_TYPE_UNDEFINED=0xffffffff
} lxdebufext_hdr_type;

typedef enum lxdebufext_type {
    LXDEBUFEXT_END=0x0,
    LXDEBUFEXT_DOVI_PTS=0x1,
    LXDEBUFEXT_DOVI_META_COMP=0x2,
    LXDEBUFEXT_DOVI_META_DM=0x3,
    LXDEBUFEXT_DOVI_PROFILEID=0x4,
    LXDEBUFEXT_DOVI_METADATA=0x5,
    LXDEBUFEXT_DOVI_MEL=0x6,
    LXDEBUFEXT_HDR_TYPE=0x10,
    LXDEBUFEXT_HDR_VUI=0x11,
    LXDEBUFEXT_HDR_MDCV=0x12,
    LXDEBUFEXT_HDR_PRIME_LUT=0x13,
    LXDEBUFEXT_HDR_PTS=0x14,
    LXDEBUFEXT_ADDITIONAL_FRAME=0x20
} lxdebufext_type;

struct hdr_vui {
    uchar transfer_characteristics;
    uchar color_primaries;
    uchar matrix_coeffs;
    uchar field_0x3;
    int video_full_range_flag;
};
typedef struct hdr_vui HDR_header;

struct hdr_sei {
    ushort disp_prim_x[3];
    ushort disp_prim_y[3];
    ushort white_point_x;
    ushort white_point_y;
    uint max_disp_mastering_luminance;
    uint min_disp_mastering_luminance;
    ushort max_content_light_level;
    ushort max_pic_average_light_level;
    uchar hdr_transfer_characteristic_idc;
    uchar field_0x1d;
    uchar field_0x1e;
    uchar field_0x1f;
};
typedef struct hdr_sei HDR_SEI;

struct _general_info {
    guint32 fourcc;
    guint32 width;
    guint32 height;
    guint32 fr_num;
    guint32 fr_den;
    guint32 trid_type;
    guint32 par_w;
    guint32 par_h;
    gint32 scan_type;
    guint32 crop_left;
    guint32 crop_right;
    guint32 crop_top;
    guint32 crop_bottom;
    gint32 actual_width;
    gint32 actual_height;
    guint brief_afd;
};
typedef struct _general_info gen_info;

struct _general_info2 {
    gint32 LR_type;
};
typedef struct _general_info2 gen_info2;

struct lxdebufext {
    lxdebufext_type type;
    guint32 size;
    guint8 *data;
};

struct _LXDEBuffer {
    guint32 hma_paddr;
    gpointer hma_vaddr;
    gint32 width;
    gint32 height;
    gint32 container_width;
    gint32 container_height;
    gint32 subsample_hor;
    gint32 subsample_ver;
    gint32 fr_num;
    gint32 fr_den;
    gint32 container_fr_num;
    gint32 container_fr_den;
    guint32 addr_y;
    guint32 stride_y;
    guint32 addr_c;
    guint32 stride_c;
    guint32 addr2_y;
    guint32 addr2_c;
    guint32 tile_base;
    gint32 crop_left;
    gint32 crop_right;
    gint32 crop_top;
    gint32 crop_bottom;
    gint32 interlace;
    gint32 multi_picture;
    gint32 trid_type;
    guint32 par_w;
    guint32 par_h;
    gint32 actual_width;
    gint32 actual_height;
    gint32 map_type;
    HDR_header hdr_header;
    HDR_SEI hdr_sei;
    guint brief_afd;
    lxdebufext_hdr_type hdr_type;
    GList * extra;
    void (* free_func)(gpointer, GstBuffer *);
    gpointer buffer_priv;
    gboolean (* origin_dispose)(GstMiniObject *);
    gpointer element;
    gpointer vdec;
    gboolean write_flag;
};
typedef struct _LXDEBuffer LXDEBuffer;

struct _LXVideoInfo {
    gen_info gen;
    gen_info2 gen2;
    HDR_header hdr_header;
    HDR_SEI hdr_sei;
    guint32 hdr_type;
};
typedef struct _LXVideoInfo LXVideoInfo;