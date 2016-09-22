/*
 * Audacious input plug-in using libxmp (Extended Module Player / XMP)
 *
 * Adapted to Audacious 3.8 API on 2016-09-18
 * Adapted to Audacious 3.6 C++ API on 2014-12-13
 * Audacious 3.0 port/rewrite for Fedora by Michael Schwendt
 * Ported for libxmp 4.0 by Claudio Matsuoka, 2013-04-13
 *
 * Originally based on:
 *   XMP plugin for XMMS/Beep/Audacious
 *   Written by Claudio Matsuoka, 2000-04-30
 *   Based on J. Nick Koston's MikMod plugin for XMMS
 */

#include <libaudcore/plugin.h>
#include <libaudcore/preferences.h>
#include <libaudcore/runtime.h>
#include <xmp.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>

#if _AUD_PLUGIN_VERSION < 48
#error "At least Audacious 3.8 is required."
#endif

#include "config.h"

#ifdef DEBUG
#else
#define _D(x...)
#endif

xmp_context ctx_play;

typedef struct {
	int mixing_freq;
	bool force_mono;
	bool interpolation;
	bool filter;
	bool convert8bit;
	bool fixloops;
	bool loop;
	bool modrange;
	double pan_amplitude;
	int time;
	struct xmp_module_info mod_info;
} XMPConfig;

XMPConfig plugin_cfg;
static bool have_xmp_ctx = false;
static void configure_load();

class AudXMP : public InputPlugin {
 public:
    static const char about[];
    static const char *const exts[];
    static const char *const defaults[];
    static const PreferencesWidget widgets[];
    static const PluginPreferences prefs;

    static constexpr PluginInfo info = {
        "XMP Plugin",
        PACKAGE,
        about,
        &prefs
    };

    static constexpr auto iinfo = InputInfo()
        .with_exts (exts);

    constexpr AudXMP() : InputPlugin(info,iinfo) {
    }

    bool init();
    bool is_our_file(const char *filename, VFSFile &file);
    bool read_tag(const char *filename, VFSFile &file, Tuple &t, Index<char> *image);
    bool play(const char *filename, VFSFile &file);
};

EXPORT AudXMP aud_plugin_instance;

const char AudXMP::about[] =
    "Extended Module Player " XMP_VERSION "\n"
    "Written by Claudio Matsuoka and Hipolito Carraro Jr.\n"
    "\n"
    "Portions Copyright (C) 1998,2000 Olivier Lapicque,\n"
    "(C) 1998 Tammo Hinrichs, (C) 1998 Sylvain Chipaux,\n"
    "(C) 1997 Bert Jahn, (C) 1999 Tatsuyuki Satoh, (C)\n"
    "1995-1999 Arnaud Carre, (C) 2001-2006 Russell Marks,\n"
    "(C) 2005-2006 Michael Kohn\n"
    "\n"
    "Plugin version " VERSION "\n"
    "Audacious 3 plugin by Michael Schwendt and Claudio Matsuoka\n"
    ;

/* Filtering files by suffix isn't good for modules. */
const char *const AudXMP::exts[] = {
    "xm", "mod", "m15", "it", "s2m", "s3m", "stm", "stx", "med", "dmf",
    "mtm", "ice", "imf", "ptm", "mdl", "ult", "liq", "psm", "amf",
    "rtm", "pt3", "tcb", "dt", "gtk", "dtt", "mgt", "digi", "dbm",
    "emod", "okt", "sfx", "far", "umx", "stim", "mtp", "ims", "669",
    "fnk", "funk", "amd", "rad", "hsc", "alm", "kris", "ksm", "unic",
    "zen", "crb", "tdd", "gmc", "gdm", "mdz", "xmz", "s3z", "j2b",
    nullptr
};

const char *const AudXMP::defaults[] = {
    "mixing_freq", "0",
    "convert8bit", "0",
    "fixloops", "0",
    "modrange", "0",
    "force_mono", "0",
    "interpolation", "1",
    "filter", "1",
    "pan_amplitude", "80",
    nullptr
};

static void strip_vfs(char *s) {
    int len;
    char *c;

    if (!s) {
        return;
    }
    _D("%s", s);
    if (!memcmp(s, "file://", 7)) {
        len = strlen(s);
        memmove(s, s + 7, len - 6);
    }

    for (c = s; *c; c++) {
        if (*c == '%' && isxdigit(*(c + 1)) && isxdigit(*(c + 2))) {
            char val[3];
            val[0] = *(c + 1);
            val[1] = *(c + 2);
            val[2] = 0;
            *c++ = strtoul(val, NULL, 16);
            len = strlen(c);
            memmove(c, c + 2, len - 1);
        }
    }
}

bool AudXMP::init(void) {
    _D("Plugin init");

    aud_config_set_defaults("XMP",AudXMP::defaults);
    configure_load();
    
    return true;
}

bool AudXMP::is_our_file(const char *_filename, VFSFile &fd) {
    char *filename = strdup(_filename);
    bool ret;

    _D("filename = %s", filename);
    strip_vfs(filename);        /* Sorry, no VFS support */

    ret = (xmp_test_module(filename, NULL) == 0);

    free(filename);
    return ret;
}

bool AudXMP::play(const char *_filename, VFSFile &fd) {
    int channelcnt;
    FILE *f;
    struct xmp_frame_info fi;
    int lret, fmt;
    char *filename = strdup(_filename);
    Tuple t;
    int freq, resol, flags, dsp;
    
    _D("play: %s\n", filename);
    strip_vfs(filename);  /* Sorry, no VFS support */
    _D("play_file: %s", filename);

    ctx_play = xmp_create_context();
    have_xmp_ctx = true;

    if ((f = fopen(filename, "rb")) == 0) {
        goto PLAY_ERROR_1;
    }
    fclose(f);

    resol = 8;
    flags = 0;
    channelcnt = 1;

    switch (plugin_cfg.mixing_freq) {
    case 1:
        freq = 22050;        /* 1:2 mixing freq */
        break;
    case 2:
        freq = 11025;        /* 1:4 mixing freq */
        break;
    default:
        freq = 44100;        /* standard mixing freq */
        break;
    }

    if (plugin_cfg.convert8bit == 0) {
        resol = 16;
    } else {
        flags |= XMP_FORMAT_8BIT;
        flags |= XMP_FORMAT_UNSIGNED;
    }

    if (plugin_cfg.force_mono == 0) {
        channelcnt = 2;
        flags &= ~XMP_FORMAT_MONO;
    } else {
        flags |= XMP_FORMAT_MONO;
    }

    if (plugin_cfg.interpolation == 1)
        xmp_set_player(ctx_play, XMP_PLAYER_INTERP, XMP_INTERP_SPLINE);
    else
        xmp_set_player(ctx_play, XMP_PLAYER_INTERP, XMP_INTERP_NEAREST);

    dsp = xmp_get_player(ctx_play, XMP_PLAYER_DSP);
    if (plugin_cfg.filter == 1)
        dsp |= XMP_DSP_LOWPASS;
    else
        dsp &= ~XMP_DSP_LOWPASS;

    xmp_set_player(ctx_play, XMP_PLAYER_MIX, plugin_cfg.pan_amplitude);

    fmt = resol == 16 ? FMT_S16_NE : FMT_U8;
    
    open_audio(fmt, freq, channelcnt);

    _D("*** loading: %s", filename);
    lret = xmp_load_module(ctx_play, filename);
    if (lret < 0) {
        goto PLAY_ERROR_1;
    }

    xmp_get_module_info(ctx_play, &plugin_cfg.mod_info);

    t.set_filename(filename);
    free(filename);
    t.set_str(Tuple::Title,plugin_cfg.mod_info.mod->name);
    t.set_str(Tuple::Codec,plugin_cfg.mod_info.mod->type);
    t.set_int(Tuple::Length,lret);
    
    xmp_start_player(ctx_play, freq, flags);

    while ( !check_stop() ) {
        int jumpToTime = check_seek();
        if ( jumpToTime != -1 ) {
            xmp_seek_time(ctx_play, jumpToTime);
        }
        
        xmp_get_frame_info(ctx_play, &fi);
        if (fi.time >= fi.total_time) {  // end reached?
            break;
        }
        write_audio(fi.buffer, fi.buffer_size);
        if (xmp_play_frame(ctx_play) != 0) {
            break;
        }
    }

    xmp_end_player(ctx_play);
    xmp_release_module(ctx_play);
    have_xmp_ctx = false;
    xmp_free_context(ctx_play);
    return true;

 PLAY_ERROR_1:
    have_xmp_ctx = false;
    xmp_free_context(ctx_play);
    free(filename);
    return false;
}
    
bool AudXMP::read_tag(const char *_filename, VFSFile &fd, Tuple &t, Index<char> *image) {
    char *filename = strdup(_filename);
    xmp_context ctx;
    struct xmp_module_info mi;
    struct xmp_frame_info fi;

    _D("filename = %s", filename);
    strip_vfs(filename);        /* Sorry, no VFS support */

    ctx = xmp_create_context();

    /* don't load samples */
    xmp_set_player(ctx, XMP_PLAYER_SMPCTL, XMP_SMPCTL_SKIP);

    if (xmp_load_module(ctx, filename) < 0) {
        free(filename);
        xmp_free_context(ctx);
        return false;
    }

    xmp_get_module_info(ctx, &mi);
    xmp_get_frame_info(ctx, &fi);

    t.set_filename(filename);
    free(filename);
    t.set_str(Tuple::Title,mi.mod->name);
    t.set_str(Tuple::Codec,mi.mod->type);
    t.set_int(Tuple::Length,fi.total_time);

    xmp_release_module(ctx);
    xmp_free_context(ctx);

    return true;
}


#define FREQ_SAMPLE_44 0
#define FREQ_SAMPLE_22 1
#define FREQ_SAMPLE_11 2

static char const configSection[] = "XMP";

static void configure_load() {

#define CFGREADINT(x) plugin_cfg.x = aud_get_int ("XMP", #x)

    CFGREADINT(mixing_freq);
    CFGREADINT(convert8bit);
    CFGREADINT(modrange);
    CFGREADINT(fixloops);
    CFGREADINT(force_mono);
    CFGREADINT(interpolation);
    CFGREADINT(filter);
    CFGREADINT(pan_amplitude);
}
    
static void configure_apply()
{
    configure_load();

    if (have_xmp_ctx) {
        // enough to make this active on-the-fly?
        xmp_set_player(ctx_play, XMP_PLAYER_MIX, plugin_cfg.pan_amplitude);
    }
}

static const PreferencesWidget precision_widgets[] = {
    WidgetRadio("16 bit", WidgetInt(configSection,"convert8bit",&configure_apply), {0}),
    WidgetRadio("8 bit", WidgetInt(configSection,"convert8bit",&configure_apply), {1}),
};

static const PreferencesWidget channels_widgets[] = {
    WidgetRadio("Stereo", WidgetInt(configSection,"force_mono",&configure_apply), {0}),
    WidgetRadio("Mono", WidgetInt(configSection,"force_mono",&configure_apply), {1}),
};

static const PreferencesWidget frequency_widgets[] = {
    WidgetRadio("44 kHz", WidgetInt(configSection,"mixing_freq",&configure_apply), {FREQ_SAMPLE_44}),
    WidgetRadio("22 kHz", WidgetInt(configSection,"mixing_freq",&configure_apply), {FREQ_SAMPLE_22}),
    WidgetRadio("11 kHz", WidgetInt(configSection,"mixing_freq",&configure_apply), {FREQ_SAMPLE_11}),
};

static const PreferencesWidget quality_widget_columns[] = {
    WidgetLabel("Resolution:"),
    WidgetBox({{precision_widgets}}),
    WidgetLabel("Channels:"),
    WidgetBox({{channels_widgets}}),
    WidgetLabel("Sampling rate:"),
    WidgetBox({{frequency_widgets}}),
};

static const PreferencesWidget options_widget_columns[] = {
    WidgetCheck("Fix sample loops", WidgetBool(plugin_cfg.fixloops,&configure_apply)),
    WidgetCheck("Force 3 octave range in standard MOD files", WidgetBool(plugin_cfg.modrange,&configure_apply)),
    WidgetCheck("Enable 32-bit linear interpolation", WidgetBool(plugin_cfg.interpolation,&configure_apply)),
    WidgetCheck("Enable IT filters", WidgetBool(plugin_cfg.filter,&configure_apply)),
    WidgetSpin("Pan amplitude (%)",WidgetFloat(plugin_cfg.pan_amplitude,&configure_apply),{0.0,100.0,1.0,""}),
};

//static const NotebookTab notebook_tabs[] = {
//    {"Quality", {quality_widget_columns}},
//    {"Options", {options_widget_columns}},
//};

static const PreferencesWidget widget_columns[] = {
    WidgetBox({{quality_widget_columns}}),
    WidgetSeparator(),
    WidgetBox({{options_widget_columns}}),
};

const PreferencesWidget AudXMP::widgets[] = {
    //WidgetNotebook ({{notebook_tabs}}),
    WidgetBox({{widget_columns}, true}),
    WidgetLabel("These settings will take effect when restarting playback.")};

const PluginPreferences AudXMP::prefs = {{widgets}};
