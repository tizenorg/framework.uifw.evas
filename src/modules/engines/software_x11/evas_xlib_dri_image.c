#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "evas_common.h"
#include "evas_xlib_dri_image.h"

static int inits = 0;
static int xfixes_ev_base = 0, xfixes_err_base = 0;
static int xfixes_major = 0, xfixes_minor = 0;
static int dri2_ev_base = 0, dri2_err_base = 0;
static int dri2_major = 0, dri2_minor = 0;
static int drm_fd = -1;
static tbm_bufmgr bufmgr = NULL;
static int exim_debug = -1;

static Eina_Bool
_drm_init(Display *disp, int scr)
{
    char *drv_name = NULL, *dev_name = NULL;
    drm_magic_t magic = 0;

    if (xfixes_lib) return EINA_TRUE;
    if ((tried) && (!xfixes_lib)) return EINA_FALSE;
    if (tried) return EINA_FALSE;
    tried = EINA_TRUE;
    drm_lib = dlopen("libdrm.so.2", RTLD_NOW | RTLD_LOCAL);
    if (!drm_lib)
        {
            ERR("Can't load libdrm.so.2");
            goto err;
        }
    lib_tbm = dlopen("libtbm.so.1", RTLD_NOW | RTLD_LOCAL);
    if (!lib_tbm)
        {
            ERR("Can't load libtbm.so.1");
            goto err;
        }
    dri_lib = dlopen("libdri2.so.0", RTLD_NOW | RTLD_LOCAL);
    if (!dri_lib)
        {
            ERR("Can't load libdri2.so.0");
            goto err;
        }

    xfixes_lib = dlopen("libXfixes.so.3", RTLD_NOW | RTLD_LOCAL);
    if (!xfixes_lib)
        {
            ERR("Can't load libXfixes.so.3");
            goto err;
        }

#define SYM(l, x) \
        do { sym_ ## x = dlsym(l, #x); \
        if (!sym_ ## x) { \
                ERR("Can't load symbol "#x); \
                goto err; \
        } \
        } while (0)

    SYM(drm_lib, drmGetMagic);

    SYM(lib_tbm, tbm_bo_import);
    SYM(lib_tbm, tbm_bo_map);
    SYM(lib_tbm, tbm_bo_unmap);
    SYM(lib_tbm, tbm_bo_unref);
    SYM(lib_tbm, tbm_bufmgr_init);
    SYM(lib_tbm, tbm_bufmgr_deinit);

    SYM(dri_lib, DRI2GetBuffers);
    SYM(dri_lib, DRI2QueryExtension);
    SYM(dri_lib, DRI2QueryVersion);
    SYM(dri_lib, DRI2Connect);
    SYM(dri_lib, DRI2Authenticate);
    SYM(dri_lib, DRI2CreateDrawable);
    SYM(dri_lib, DRI2DestroyDrawable);

    SYM(xfixes_lib, XFixesQueryExtension);
    SYM(xfixes_lib, XFixesQueryVersion);
    SYM(xfixes_lib, XFixesCreateRegion);
    SYM(xfixes_lib, XFixesDestroyRegion);
    if (!sym_XFixesQueryExtension(disp, &xfixes_ev_base, &xfixes_err_base))
        {
            if (exim_debug) ERR("XFixes extension not in xserver");
            goto err;
        }
    sym_XFixesQueryVersion(disp, &xfixes_major, &xfixes_minor);


    if (!sym_DRI2QueryExtension(disp, &dri2_ev_base, &dri2_err_base))
        {
            if (exim_debug) ERR("DRI2 extension not in xserver");
            goto err;
        }
    if (!sym_DRI2QueryVersion(disp, &dri2_major, &dri2_minor))
        {
            if (exim_debug) ERR("DRI2 query version failed");
            goto err;
        }
    if (dri2_minor < 99)
        {
            if (exim_debug) ERR("Not supported by DRI2 version(%i.%i)",
                                dri2_major, dri2_minor);
            goto err;
        }
    if (!sym_DRI2Connect(disp, RootWindow(disp, scr), &drv_name, &dev_name))
        {
            if (exim_debug) ERR("DRI2 connect failed on screen %i", scr);
            goto err;
        }
    drm_fd = open(dev_name, O_RDWR);
    if (drm_fd < 0)
        {
            if (exim_debug) ERR("DRM FD open of '%s' failed", dev_name);
            goto err;
        }
    if (sym_drmGetMagic(drm_fd, &magic))
        {
            if (exim_debug) ERR("DRM get magic failed");
            goto err;
        }
    if (!sym_DRI2Authenticate(disp, RootWindow(disp, scr),
                              (unsigned int)magic))
        {
            if (exim_debug) ERR("DRI2 authenticate failed with magic 0x%x on screen %i", (unsigned int)magic, scr);
            goto err;
        }
    if (!(bufmgr = sym_tbm_bufmgr_init(drm_fd)))
        {
            if (exim_debug) ERR("DRM bufmgr init failed");
            goto err;
        }
    if (drv_name)
        {
            XFree(drv_name);
        }
    if (dev_name)
        {
            XFree(dev_name);
        }
    return EINA_TRUE;
    err:
    if (drm_fd >= 0)
        {
            close(drm_fd);
            drm_fd = -1;
        }
    if (drm_lib)
        {
            dlclose(drm_lib);
            drm_lib = NULL;
        }
    if (lib_tbm)
        {
            dlclose(lib_tbm);
            lib_tbm = NULL;
        }
    if (dri_lib)
        {
            dlclose(dri_lib);
            dri_lib = NULL;
        }
    if (xfixes_lib)
        {
            dlclose(xfixes_lib);
            xfixes_lib = NULL;
        }
    if (drv_name)
        {
            XFree(drv_name);
        }
    if (dev_name)
        {
            XFree(dev_name);
        }
    return EINA_FALSE;
}

static void
_drm_shutdown(void)
{
    return;
    // leave this here as notation on how to shut down stuff - never do it
    // though, as once shut down, we have to re-init and this could be
    // expensive especially if u have a single canvas that is changing config
    // and being shut down and re-initted a few times.
    if (bufmgr)
        {
            sym_tbm_bufmgr_deinit(bufmgr);
            bufmgr = NULL;
        }
    if (drm_fd >= 0) close(drm_fd);
    drm_fd = -1;
    dlclose(lib_tbm);
    lib_tbm = NULL;
    dlclose(dri_lib);
    dri_lib = NULL;
    dlclose(xfixes_lib);
    xfixes_lib = NULL;
}

static Eina_Bool
_drm_setup(Display *disp, Evas_DRI_Image *exim)
{
    sym_DRI2CreateDrawable(disp, exim->draw);
    return EINA_TRUE;
}

static void
_drm_cleanup(Evas_DRI_Image *exim)
{
    sym_DRI2DestroyDrawable(exim->dis, exim->draw);
}

Eina_Bool
evas_xlib_image_dri_init(Evas_DRI_Image *exim,
                         Display *display)
{
    exim->dis = display;
    if (inits <= 0)
        {
            if(!_drm_init(display, 0)) return EINA_FALSE;
        }
    inits++;

    if(!_drm_setup(display, exim))
        {
            inits--;
            if (inits == 0) _drm_shutdown();
            free(exim);
            return EINA_FALSE;
        }
    return EINA_TRUE;
}

Eina_Bool
evas_xlib_image_dri_used()
{
    if(inits > 0)
        {
            return EINA_TRUE;
        }
    return EINA_FALSE;
}

Eina_Bool
evas_xlib_image_get_buffers(RGBA_Image *im)
{
    DRI_Native *n = NULL;
    Display *d;
    Evas_DRI_Image *exim;

    if (im->native.data)
        n = im->native.data;
    if (!n)
        return EINA_FALSE;

    exim = n->exim;
    d = n->d;

    unsigned int attach = DRI2BufferFrontLeft;
    int num;
    tbm_bo_handle bo_handle;
    DRI2BufferFlags *flags;

    exim->buf = sym_DRI2GetBuffers(d, exim->draw,
                                   &(exim->buf_w), &(exim->buf_h),
                                   &attach, 1, &num);
    if (!exim->buf) return EINA_FALSE;
    if (!exim->buf->name) return EINA_FALSE;

    flags = (DRI2BufferFlags *)(&(exim->buf->flags));
    if (!flags->data.is_reused)
        {
            if (exim_debug) DBG("Buffer cache not reused - clear cache\n");
            if (exim->buf_cache)
                {
                    sym_tbm_bo_unref(exim->buf_cache->buf_bo);
                }
        }
    else
        {
            if (exim->buf_cache && exim->buf_cache->name == exim->buf->name)
                {
                    if (exim_debug) DBG("Cached buf name %i found\n", exim->buf_cache->name);
                    exim->buf_bo = exim->buf_cache->buf_bo;
                }
        }

    if (!exim->buf_bo)
        {
            exim->buf_bo = sym_tbm_bo_import(bufmgr, exim->buf->name);
            if (!exim->buf_bo) return EINA_FALSE;
            // cache the buf entry
            exim->buf_cache = calloc(1, sizeof(Buffer));
            exim->buf_cache->name = exim->buf->name;
            exim->buf_cache->buf_bo = exim->buf_bo;
            if (exim_debug) DBG("Buffer cache added name %i\n", exim->buf_cache->name);
        }

    bo_handle = sym_tbm_bo_map (exim->buf_bo, TBM_DEVICE_CPU, TBM_OPTION_READ |TBM_OPTION_WRITE);
    exim->buf_data = bo_handle.ptr;

    if (!exim->buf_data)
        {
            ERR("Buffer map name %i failed", exim->buf->name);
            return EINA_FALSE;
        }

    int *bpl = &exim->bpl;
    if (bpl) *bpl = exim->buf->pitch;

    exim->w = exim->buf_w;
    exim->h = exim->buf_h;
    im->image.data = exim->buf_data;

    evas_xlib_image_buffer_unmap(exim);
    return EINA_TRUE;
}

void
evas_xlib_image_buffer_unmap(Evas_DRI_Image *exim)
{
    sym_tbm_bo_unmap(exim->buf_bo);
    if (exim_debug) DBG("Unmap buffer name %i\n", exim->buf->name);
    free(exim->buf);
    exim->buf = NULL;
    exim->buf_bo = NULL;
    exim->buf_data = NULL;
}


void
evas_xlib_image_dri_free(Evas_DRI_Image *exim)
{
    if (exim->buf_cache)
        {
            if (exim_debug) DBG("Cached buf name %i freed\n", exim->buf_cache->name);
            sym_tbm_bo_unref(exim->buf_cache->buf_bo);
            free(exim->buf_cache);
        }
    _drm_cleanup(exim);
    free(exim);
    inits--;
    if (inits == 0) _drm_shutdown();
}

Evas_DRI_Image *
evas_xlib_image_dri_new(int w, int h, Visual *vis, int depth)
{
    Evas_DRI_Image *exim;

    exim = calloc(1, sizeof(Evas_DRI_Image));
    if (!exim)
        return NULL;

    exim->w = w;
    exim->h = h;
    exim->visual = vis;
    exim->depth = depth;
    return exim;
}


static void
_native_free_cb(void *data EINA_UNUSED, void *image)
{
    RGBA_Image *im = image;
    DRI_Native *n = im->native.data;

    //TODO: deal with pixmap hash
    if (n->exim)
        {
            evas_xlib_image_dri_free(n->exim);
            n->exim = NULL;
        }
    n->visual = NULL;
    n->d = NULL;

    im->native.data        = NULL;
    im->native.func.data   = NULL;
    im->native.func.free   = NULL;
    im->image.data         = NULL;
    free(n);
}

void *
evas_xlib_image_dri_native_set(void *data, void *image, void *native)
{
    Display *d = NULL;
    Visual  *vis = NULL;
    Pixmap   pm = 0;
    DRI_Native  *n = NULL;
    RGBA_Image *im = image;
    int w, h;
    Evas_DRI_Image *exim;
    Evas_Native_Surface *ns = native;
    Outbuf *ob = (Outbuf *)data;

    Window wdum;
    int idum;
    unsigned int uidum, depth = 0;

    d = ob->priv.x11.xlib.disp;
    vis = ns->data.x11.visual;
    pm = ns->data.x11.pixmap;

    // fixme: round trip :(
    // get pixmap depth info
    XGetGeometry(d, pm, &wdum, &idum, &idum, &uidum, &uidum, &uidum, &depth);

    //TODO: deal with pixmap cache
    w = im->cache_entry.w;
    h = im->cache_entry.h;

    exim = evas_xlib_image_dri_new(w, h, vis, depth);

    if (!exim)
        {
            ERR("evas_xlib_image_dri_new failed.");
            return EINA_FALSE;
        }

    if (ns->type == EVAS_NATIVE_SURFACE_X11)
        {
            exim->draw = (Drawable)ns->data.x11.pixmap;
        }

    if (ns)
        {
            n = calloc(1, sizeof(DRI_Native));
            if (n)
                {
                    memcpy(&(n->ns), ns, sizeof(Evas_Native_Surface));
                    n->pixmap = pm;
                    n->visual = vis;
                    n->d = d;
                    n->exim = exim;
                    im->native.data = n;
                    im->native.func.data = NULL;
                    im->native.func.free = _native_free_cb;
                }
        }

    if (evas_xlib_image_dri_init(exim, d))
        {
            evas_xlib_image_get_buffers(im);
        }
    else return EINA_FALSE;
    return im;
}
