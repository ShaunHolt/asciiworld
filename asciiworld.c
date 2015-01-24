#include <math.h>
#include <shapefil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#define PIXEL_NORMAL 1
#define PIXEL_HIGHLIGHT 2
#define PIXEL_DARK 3
#define PIXEL_SUN 4

struct screen
{
    int width, height;
    char *data;

    struct sun
    {
        int active;
        double lon, lat;
        double x, y, z;
    } sun;
};

double
project_x(struct screen *s, double x)
{
    return (x + 180) / 360 * s->width;
}

double
project_y(struct screen *s, double y)
{
    return (180 - (y + 90)) / 180 * s->height;
}

double
unproject_x(struct screen *s, double px)
{
    return (px * 360) / s->width - 180;
}

double
unproject_y(struct screen *s, double py)
{
    return 90 - (py * 180) / s->height;
}

void
calc_sun(struct sun *sun)
{
    time_t t;
    struct tm utc;

    time(&t);
    gmtime_r(&t, &utc);

    /* See german notes in "sonnenstand.txt". */
    sun->lon = ((utc.tm_hour * 60 * 60 + utc.tm_min * 60 + utc.tm_sec) -
                (86400.0 / 2 + ((-0.171 * sin(0.0337 * (utc.tm_yday + 1) + 0.465) -
                 0.1299 * sin(0.01787 * (utc.tm_yday + 1) - 0.168)) * -3600))) *
               (-360.0 / 86400);
    sun->lat = 0.4095 * sin(0.016906 * ((utc.tm_yday + 1) - 80.086)) * 180 / M_PI;

    /* Precalc cartesian coordinates. */
    sun->x = sin((sun->lat + 90) * M_PI / 180) * cos(sun->lon * M_PI / 180);
    sun->y = sin((sun->lat + 90) * M_PI / 180) * sin(sun->lon * M_PI / 180);
    sun->z = cos((sun->lat + 90) * M_PI / 180);
}

char
decide_shade(struct screen *s, int x, int y)
{
    double lon, lat;
    double px, py, pz;
    double sp;

    if (!s->sun.active)
        return PIXEL_NORMAL;

    /* Unproject the point, calc cartesian coordinates of it and then
     * use the scalar product to determine whether this point lies on
     * the same half of the earth as the "sun point". */

    lon = unproject_x(s, x);  /* phi */
    lat = unproject_y(s, y);  /* theta */

    px = sin((lat + 90) * M_PI / 180) * cos(lon * M_PI / 180);
    py = sin((lat + 90) * M_PI / 180) * sin(lon * M_PI / 180);
    pz = cos((lat + 90) * M_PI / 180);

    sp = px * s->sun.x + py * s->sun.y + pz * s->sun.z;

    if (sp < 0)
        return PIXEL_DARK;
    else
        return PIXEL_NORMAL;
}

int
screen_init(struct screen *s, int width, int height)
{
    s->width = width;
    s->height = height;
    s->data = calloc(1, width * height);
    if (s->data == NULL)
    {
        fprintf(stderr, "Out of memory in screen_init()\n");
        return 0;
    }
    return 1;
}

void
screen_show_interpreted(struct screen *s, int trailing_newline)
{
    int x, y, sun_found;
    char a, b, c, d;

    for (y = 0; y < s->height - 1; y += 2)
    {
        for (x = 0; x < s->width - 1; x += 2)
        {
            a = s->data[y * s->width + x];
            b = s->data[y * s->width + x + 1];
            c = s->data[(y + 1) * s->width + x];
            d = s->data[(y + 1) * s->width + x + 1];

            if (a == PIXEL_HIGHLIGHT || b == PIXEL_HIGHLIGHT ||
                c == PIXEL_HIGHLIGHT || d == PIXEL_HIGHLIGHT)
                printf("\033[31;1mX\033[0m");
            else
            {
                sun_found = 0;

                if (s->sun.active)
                {
                    if (a == PIXEL_SUN || b == PIXEL_SUN ||
                        c == PIXEL_SUN || d == PIXEL_SUN)
                    {
                        sun_found = 1;
                        printf("\033[36;1mS\033[0m");
                    }
                    else if (a == PIXEL_DARK || b == PIXEL_DARK ||
                             c == PIXEL_DARK || d == PIXEL_DARK)
                        printf("\033[30;1m");
                    else
                        printf("\033[33m");
                }

                if (!sun_found)
                {
                    if (!a && !b && !c && !d)
                        printf(" ");

                    else if (!a && !b && !c &&  d)
                        printf(".");
                    else if (!a && !b &&  c && !d)
                        printf(",");
                    else if (!a && !b &&  c &&  d)
                        printf("_");
                    else if (!a &&  b && !c && !d)
                        printf("'");
                    else if (!a &&  b && !c &&  d)
                        printf("|");
                    else if (!a &&  b &&  c && !d)
                        printf("/");
                    else if (!a &&  b &&  c &&  d)
                        printf("J");

                    else if ( a && !b && !c && !d)
                        printf("`");
                    else if ( a && !b && !c &&  d)
                        printf("\\");
                    else if ( a && !b &&  c && !d)
                        printf("|");
                    else if ( a && !b &&  c &&  d)
                        printf("L");
                    else if ( a &&  b && !c && !d)
                        printf("\"");
                    else if ( a &&  b && !c &&  d)
                        printf("7");
                    else if ( a &&  b &&  c && !d)
                        printf("r");

                    else if ( a &&  b &&  c &&  d)
                        printf("#");

                    if (s->sun.active)
                        printf("\033[0m");
                }
            }
        }
        if (trailing_newline || y + 1 < s->height - 1)
            printf("\n");
    }
}

void
screen_set_pixel(struct screen *s, int x, int y, char val)
{
    if (x >= 0 && y >= 0 && x < s->width && y < s->height)
        s->data[y * s->width + x] = val;
}

void
screen_draw_line(struct screen *s, int x1, int y1, int x2, int y2)
{
    int x, y, t, dx, dy, incx, incy, pdx, pdy, es, el, err;

    dx = x2 - x1;
    dy = y2 - y1;

    incx = dx < 0 ? -1 : 1;
    incy = dy < 0 ? -1 : 1;

    dx = dx < 0 ? -dx : dx;
    dy = dy < 0 ? -dy : dy;

    if (dx > dy)
    {
        pdx = incx;
        pdy = 0;
        es = dy;
        el = dx;
    }
    else
    {
        pdx = 0;
        pdy = incy;
        es = dx;
        el = dy;
    }

    x = x1;
    y = y1;
    err = el / 2;
    screen_set_pixel(s, x, y, decide_shade(s, x, y));

    for (t = 0; t < el; t++)
    {
        err -= es;
        if (err < 0)
        {
            err += el;
            x += incx;
            y += incy;
        }
        else
        {
            x += pdx;
            y += pdy;
        }
        screen_set_pixel(s, x, y, decide_shade(s, x, y));
    }
}

void
screen_draw_line_projected(struct screen *s, int x1, int y1, int x2, int y2)
{
    double sx1, sy1, sx2, sy2;

    /* y is flipped */

    sx1 = project_x(s, x1);
    sy1 = project_y(s, y1);
    sx2 = project_x(s, x2);
    sy2 = project_y(s, y2);

    if ((int)sx1 == (int)sx2 && (int)sy1 == (int)sy2)
        return;

    screen_draw_line(s, sx1, sy1, sx2, sy2);
}

int
screen_draw_map(struct screen *s, char *file)
{
    int ret = 1;
    int i, n, t, p, v, isFirst;
    double x1, y1;
    SHPHandle h;
    SHPObject *o;

    if (s->sun.active)
        calc_sun(&s->sun);

    h = SHPOpen(file, "rb");
    if (h == NULL)
    {
        fprintf(stderr, "Could not open shapefile\n");
        ret = 0;
        goto out;
    }

    SHPGetInfo(h, &n, &t, NULL, NULL);

    if (t != SHPT_POLYGON)
    {
        fprintf(stderr, "This is not a polygon file\n");
        ret = 0;
        goto cleanout;
    }

    for (i = 0; i < n; i++)
    {
        o = SHPReadObject(h, i);
        if (o == NULL)
        {
            fprintf(stderr, "Could not read object %d\n", i);
            ret = 0;
            goto cleanout;
        }

        if (o->nSHPType != SHPT_POLYGON)
        {
            fprintf(stderr, "Shape %d is not a polygon", i);
            ret = 0;
            goto cleanout;
        }

        v = 0;
        p = 0;
        isFirst = 1;

        while (v < o->nVertices)
        {
            if (p < o->nParts && v == o->panPartStart[p])
            {
                /* Start of part "p" */
                isFirst = 1;
                p++;
            }
            if (!isFirst)
            {
                /* y is flipped */
                screen_draw_line_projected(s, x1, y1, o->padfX[v], o->padfY[v]);
            }
            x1 = o->padfX[v];
            y1 = o->padfY[v];
            isFirst = 0;
            v++;
        }

        SHPDestroyObject(o);
    }

cleanout:
    SHPClose(h);
out:
    return ret;
}

int
screen_mark_locations(struct screen *s, char *file)
{
    FILE *fp;
    int scanret = 0;
    double lat, lon, sx, sy;

    fp = fopen(file, "r");
    if (fp == NULL)
    {
        fprintf(stderr, "Could not open locations file\n");
        return 0;
    }

    while (1)
    {
        scanret = fscanf(fp, "%lf %lf\n", &lat, &lon);

        if (scanret == EOF)
            break;

        if (scanret == 2)
        {
            sx = project_x(s, lon);
            sy = project_y(s, lat);

            s->data[(int)sy * s->width + (int)sx] = PIXEL_HIGHLIGHT;
        }
    }

    fclose(fp);

    return 1;
}

void
screen_mark_sun(struct screen *s)
{
    double sx, sy;

    sx = project_x(s, s->sun.lon);
    sy = project_y(s, s->sun.lat);

    s->data[(int)sy * s->width + (int)sx] = PIXEL_SUN;
}

int
main(int argc, char **argv)
{
    struct screen s;
    struct winsize w;
    int trailing_newline = 1;
    int opt;
    char *map = "ne_110m_land.shp";
    char *highlight_locations = NULL;

    s.sun.active = 0;

    if (isatty(STDOUT_FILENO))
    {
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    }
    else
    {
        w.ws_col = 80;
        w.ws_row = 24;
    }

    while ((opt = getopt(argc, argv, "w:h:m:l:sT")) != -1)
    {
        switch (opt)
        {
            case 'w':
                w.ws_col = atoi(optarg);
                break;
            case 'h':
                w.ws_row = atoi(optarg);
                break;
            case 'm':
                map = optarg;
                break;
            case 'l':
                highlight_locations = optarg;
                break;
            case 's':
                s.sun.active = 1;
                break;
            case 'T':
                trailing_newline = 0;
                break;
            default:
                exit(EXIT_FAILURE);
        }
    }

    if (!screen_init(&s, 2 * w.ws_col, 2 * w.ws_row))
        exit(EXIT_FAILURE);
    if (!screen_draw_map(&s, map))
        exit(EXIT_FAILURE);
    if (highlight_locations != NULL)
        if (!screen_mark_locations(&s, highlight_locations))
            exit(EXIT_FAILURE);
    if (s.sun.active)
        screen_mark_sun(&s);
    screen_show_interpreted(&s, trailing_newline);

    exit(EXIT_SUCCESS);
}
