/* PDCurses */

#include "pdcsdl.h"

#include <stdlib.h>
#ifndef PDC_WIDE
#include "../common/font437.h"
#endif
#include "../common/iconbmp.h"

#ifdef PDC_WIDE
# ifndef PDC_FONT_PATH
#  define PDC_FONT_PATH "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"
# endif
TTF_Font *pdc_ttffont = NULL;
int pdc_font_size = 17;
#endif

SDL_Surface *pdc_screen = NULL, *pdc_font = NULL, *pdc_icon = NULL,
            *pdc_back = NULL, *pdc_tileback = NULL;
int pdc_sheight = 0, pdc_swidth = 0, pdc_yoffset = 0, pdc_xoffset = 0;

SDL_Color pdc_color[PDC_MAXCOL];
Uint32 pdc_mapped[PDC_MAXCOL];
int pdc_fheight, pdc_fwidth, pdc_fthick, pdc_flastc;
bool pdc_own_screen;

static int max_height, max_width;

static void _clean(void)
{
#ifdef PDC_WIDE
    if (pdc_ttffont)
    {
        TTF_CloseFont(pdc_ttffont);
        TTF_Quit();
    }
#endif
    SDL_FreeSurface(pdc_tileback);
    SDL_FreeSurface(pdc_back);
    SDL_FreeSurface(pdc_icon);
    SDL_FreeSurface(pdc_font);

    SDL_Quit();
}

void PDC_retile(void)
{
    if (pdc_tileback)
        SDL_FreeSurface(pdc_tileback);

    pdc_tileback = SDL_DisplayFormatAlpha(pdc_screen);
    if (pdc_tileback == NULL)
        return;

    if (pdc_back)
    {
        SDL_Rect dest;

        dest.y = 0;

        while (dest.y < pdc_tileback->h)
        {
            dest.x = 0;

            while (dest.x < pdc_tileback->w)
            {
                SDL_BlitSurface(pdc_back, 0, pdc_tileback, &dest);
                dest.x += pdc_back->w;
            }

            dest.y += pdc_back->h;
        }

        SDL_BlitSurface(pdc_tileback, 0, pdc_screen, 0);
    }
}

void PDC_scr_close(void)
{
    PDC_LOG(("PDC_scr_close() - called\n"));
}

void PDC_scr_free(void)
{
}

static void _initialize_colors(void)
{
    int i, r, g, b;

    for (i = 0; i < 8; i++)
    {
        pdc_color[i].r = (i & COLOR_RED) ? 0xc0 : 0;
        pdc_color[i].g = (i & COLOR_GREEN) ? 0xc0 : 0;
        pdc_color[i].b = (i & COLOR_BLUE) ? 0xc0 : 0;

        pdc_color[i + 8].r = (i & COLOR_RED) ? 0xff : 0x40;
        pdc_color[i + 8].g = (i & COLOR_GREEN) ? 0xff : 0x40;
        pdc_color[i + 8].b = (i & COLOR_BLUE) ? 0xff : 0x40;
    }

    /* 256-color xterm extended palette: 216 colors in a 6x6x6 color
       cube, plus 24 shades of gray */

    for (i = 16, r = 0; r < 6; r++)
        for (g = 0; g < 6; g++)
            for (b = 0; b < 6; b++, i++)
            {
                pdc_color[i].r = (r ? r * 40 + 55 : 0);
                pdc_color[i].g = (g ? g * 40 + 55 : 0);
                pdc_color[i].b = (b ? b * 40 + 55 : 0);
            }

    for (i = 232; i < 256; i++)
        pdc_color[i].r = pdc_color[i].g = pdc_color[i].b = (i - 232) * 10 + 8;

    for (i = 0; i < 256; i++)
        pdc_mapped[i] = SDL_MapRGB(pdc_screen->format, pdc_color[i].r,
                                   pdc_color[i].g, pdc_color[i].b);
}

/* open the physical screen -- miscellaneous initialization */

int PDC_scr_open(void)
{
    PDC_LOG(("PDC_scr_open() - called\n"));

    pdc_own_screen = !pdc_screen;

    if (pdc_own_screen)
    {
        if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER) < 0)
        {
            fprintf(stderr, "Could not start SDL: %s\n", SDL_GetError());
            return ERR;
        }

        atexit(_clean);
    }

#ifdef PDC_WIDE
    if (!pdc_ttffont)
    {
        const char *ptsz, *fname;

        if (TTF_Init() == -1)
        {
            fprintf(stderr, "Could not start SDL_TTF: %s\n", SDL_GetError());
            return ERR;
        }

        ptsz = getenv("PDC_FONT_SIZE");
        if (ptsz != NULL)
           pdc_font_size = atoi(ptsz);
        if (pdc_font_size <= 0)
           pdc_font_size = 18;

        fname = getenv("PDC_FONT");
        pdc_ttffont = TTF_OpenFont(fname ? fname : PDC_FONT_PATH,
                                   pdc_font_size);
    }

    if (!pdc_ttffont)
    {
        fprintf(stderr, "Could not load font\n");
        return ERR;
    }

    TTF_SetFontKerning(pdc_ttffont, 0);
    TTF_SetFontHinting(pdc_ttffont, TTF_HINTING_MONO);

    SP->mono = FALSE;
#else
    if (!pdc_font)
    {
        const char *fname = getenv("PDC_FONT");
        pdc_font = SDL_LoadBMP(fname ? fname : "pdcfont.bmp");
    }

    if (!pdc_font)
        pdc_font = SDL_LoadBMP_RW(SDL_RWFromMem(font437, sizeof(font437)), 0);

    if (!pdc_font)
    {
        fprintf(stderr, "Could not load font\n");
        return ERR;
    }

    SP->mono = !pdc_font->format->palette;
#endif

    if (!SP->mono && !pdc_back)
    {
        const char *bname = getenv("PDC_BACKGROUND");
        pdc_back = SDL_LoadBMP(bname ? bname : "pdcback.bmp");
    }

    if (!SP->mono && (pdc_back || !pdc_own_screen))
    {
        SP->orig_attr = TRUE;
        SP->orig_fore = COLOR_WHITE;
        SP->orig_back = -1;
    }
    else
        SP->orig_attr = FALSE;

#ifdef PDC_WIDE
    TTF_SizeText(pdc_ttffont, "W", &pdc_fwidth, &pdc_fheight);
    pdc_fthick = pdc_font_size / 20 + 1;
#else
    pdc_fheight = pdc_font->h / 8;
    pdc_fwidth = pdc_font->w / 32;
    pdc_fthick = 1;

    if (!SP->mono)
        pdc_flastc = pdc_font->format->palette->ncolors - 1;
#endif

    if (pdc_own_screen && !pdc_icon)
    {
        const char *iname = getenv("PDC_ICON");
        pdc_icon = SDL_LoadBMP(iname ? iname : "pdcicon.bmp");

        if (!pdc_icon)
            pdc_icon = SDL_LoadBMP_RW(SDL_RWFromMem(iconbmp,
                                                    sizeof(iconbmp)), 0);

        if (pdc_icon)
            SDL_WM_SetIcon(pdc_icon, NULL);
    }

    if (pdc_own_screen)
    {
        const SDL_VideoInfo *info = SDL_GetVideoInfo();
        max_height = info->current_h;
        max_width = info->current_w;

        const char *env = getenv("PDC_LINES");
        pdc_sheight = (env ? atoi(env) : 25) * pdc_fheight;

        env = getenv("PDC_COLS");
        pdc_swidth = (env ? atoi(env) : 80) * pdc_fwidth;

        pdc_screen = SDL_SetVideoMode(pdc_swidth, pdc_sheight, 0,
            SDL_SWSURFACE|SDL_ANYFORMAT|SDL_RESIZABLE);
    }
    else
    {
        if (!pdc_sheight)
            pdc_sheight = pdc_screen->h - pdc_yoffset;

        if (!pdc_swidth)
            pdc_swidth = pdc_screen->w - pdc_xoffset;
    }

    if (!pdc_screen)
    {
        fprintf(stderr, "Couldn't create a surface: %s\n", SDL_GetError());
        return ERR;
    }

    if (SP->orig_attr)
        PDC_retile();

    _initialize_colors();

    SDL_EnableUNICODE(1);

    PDC_mouse_set();

    if (pdc_own_screen)
        PDC_set_title("PDCurses");

    SP->mouse_wait = PDC_CLICK_PERIOD;
    SP->audible = FALSE;

    SP->termattrs = A_COLOR | A_UNDERLINE | A_LEFT | A_RIGHT | A_REVERSE;
#ifdef PDC_WIDE
    SP->termattrs |= A_ITALIC;
#endif

    PDC_reset_prog_mode();

    return OK;
}

/* the core of resize_term() */

int PDC_resize_screen(int nlines, int ncols)
{
    if (!pdc_own_screen)
        return ERR;

    if (nlines && ncols)
    {
        while (nlines * pdc_fheight > max_height)
            nlines--;
        pdc_sheight = nlines * pdc_fheight;
        while (ncols * pdc_fwidth > max_width)
            ncols--;
        pdc_swidth = ncols * pdc_fwidth;
    }

    SDL_FreeSurface(pdc_screen);

    pdc_screen = SDL_SetVideoMode(pdc_swidth, pdc_sheight, 0,
        SDL_SWSURFACE|SDL_ANYFORMAT|SDL_RESIZABLE);

    if (pdc_tileback)
        PDC_retile();

    return OK;
}

void PDC_reset_prog_mode(void)
{
    PDC_LOG(("PDC_reset_prog_mode() - called.\n"));

    PDC_flushinp();
    SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
}

void PDC_reset_shell_mode(void)
{
    PDC_LOG(("PDC_reset_shell_mode() - called.\n"));

    SDL_EnableKeyRepeat(0, 0);
    PDC_flushinp();
}

void PDC_restore_screen_mode(int i)
{
}

void PDC_save_screen_mode(int i)
{
}

bool PDC_can_change_color(void)
{
    return TRUE;
}

int PDC_color_content(short color, short *red, short *green, short *blue)
{
    *red = DIVROUND(pdc_color[color].r * 1000, 255);
    *green = DIVROUND(pdc_color[color].g * 1000, 255);
    *blue = DIVROUND(pdc_color[color].b * 1000, 255);

    return OK;
}

int PDC_init_color(short color, short red, short green, short blue)
{
    pdc_color[color].r = DIVROUND(red * 255, 1000);
    pdc_color[color].g = DIVROUND(green * 255, 1000);
    pdc_color[color].b = DIVROUND(blue * 255, 1000);

    pdc_mapped[color] = SDL_MapRGB(pdc_screen->format, pdc_color[color].r,
                                   pdc_color[color].g, pdc_color[color].b);

    return OK;
}
