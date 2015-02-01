#include "evas_engine.h"

# include <dlfcn.h>      /* dlopen,dlclose,etc */
# include <sys/types.h>
# include <sys/stat.h>
# include <fcntl.h>
static Eina_Bool tried = EINA_FALSE;
////////////////////////////////////
//libdrm.so.2
static void *drm_lib = NULL;

typedef unsigned int drm_magic_t;
static int (*sym_drmGetMagic) (int fd, drm_magic_t *magic) = NULL;

////////////////////////////////////
// libtbm.so.1
#define TBM_DEVICE_CPU 1
#define TBM_OPTION_READ     (1 << 0)
#define TBM_OPTION_WRITE    (1 << 1)
static void *lib_tbm = NULL;

typedef struct _tbm_bufmgr *tbm_bufmgr;
typedef struct _tbm_bo *tbm_bo;

typedef union _tbm_bo_handle
{
    void     *ptr;
    int32_t  s32;
    uint32_t u32;
    int64_t  s64;
    uint64_t u64;
} tbm_bo_handle;

static tbm_bo (*sym_tbm_bo_import) (tbm_bufmgr bufmgr, unsigned int key) = NULL;
static tbm_bo_handle (*sym_tbm_bo_map) (tbm_bo bo, int device, int opt) = NULL;
static int (*sym_tbm_bo_unmap)  (tbm_bo bo) = NULL;
static void (*sym_tbm_bo_unref) (tbm_bo bo) = NULL;
static tbm_bufmgr (*sym_tbm_bufmgr_init) (int fd) = NULL;
static void (*sym_tbm_bufmgr_deinit) (tbm_bufmgr bufmgr) = NULL;
////////////////////////////////////
// libdri2.so.0
#define DRI2BufferFrontLeft 0
static void *dri_lib = NULL;

typedef unsigned long long CD64;

typedef struct
{
    unsigned int attachment;
    unsigned int name;
    unsigned int pitch;
    unsigned int cpp;
    unsigned int flags;
} DRI2Buffer;

#define DRI2_BUFFER_TYPE_WINDOW 0x0
#define DRI2_BUFFER_TYPE_PIXMAP 0x1
#define DRI2_BUFFER_TYPE_FB     0x2

typedef union
{
    unsigned int flags;
    struct
    {
        unsigned int type:1;
        unsigned int is_framebuffer:1;
        unsigned int is_mapped:1;
        unsigned int is_reused:1;
        unsigned int idx_reuse:3;
    }
    data;
} DRI2BufferFlags;

static DRI2Buffer *(*sym_DRI2GetBuffers) (Display *display, XID drawable, int *width, int *height, unsigned int *attachments, int count, int *outCount) = NULL;
static Bool (*sym_DRI2QueryExtension) (Display *display, int *eventBase, int *errorBase) = NULL;
static Bool (*sym_DRI2QueryVersion) (Display *display, int *major, int *minor) = NULL;
static Bool (*sym_DRI2Connect) (Display *display, XID window, char **driverName, char **deviceName) = NULL;
static Bool (*sym_DRI2Authenticate) (Display *display, XID window, unsigned int magic) = NULL;
static void (*sym_DRI2CreateDrawable) (Display *display, XID drawable) = NULL;
static void (*sym_DRI2DestroyDrawable) (Display *display, XID handle) = NULL;

////////////////////////////////////
// libXfixes.so.3
static void *xfixes_lib = NULL;

static Bool (*sym_XFixesQueryExtension) (Display *display, int *event_base_return, int *error_base_return) = NULL;
static Status (*sym_XFixesQueryVersion) (Display *display, int *major_version_return, int *minor_version_return) = NULL;
static XID (*sym_XFixesCreateRegion) (Display *display, XRectangle *rectangles, int nrectangles) = NULL;
static void (*sym_XFixesDestroyRegion) (Display *dpy, XID region) = NULL;

typedef struct
{
    unsigned int name;
    tbm_bo   buf_bo;
} Buffer;

typedef struct _Evas_DRI_Image Evas_DRI_Image;
typedef struct _DRI_Native DRI_Native;

struct _Evas_DRI_Image
{
    Display         *dis;
    Visual          *visual;
    int              depth;
    int              w, h;
    int              bpl, bpp, rows;
    unsigned char   *data;
    Drawable        draw;
    tbm_bo          buf_bo;
    DRI2Buffer      *buf;
    void            *buf_data;
    int             buf_w, buf_h;
    Buffer          *buf_cache;
};

struct _DRI_Native
{
    Evas_Native_Surface ns;
    Pixmap              pixmap;
    Visual             *visual;
    Display            *d;

    Evas_DRI_Image       *exim;
};

Evas_DRI_Image *evas_xlib_image_dri_new(int w, int h, Visual *vis, int depth);

void evas_xlib_image_dir_free(Evas_DRI_Image *exim);
Eina_Bool evas_xlib_image_get_buffers(RGBA_Image *im);
Eina_Bool evas_xlib_image_dri_init(Evas_DRI_Image *exim, Display *display);
Eina_Bool evas_xlib_image_dri_used();
void *evas_xlib_image_dri_native_set(void *data, void *image, void *native);
