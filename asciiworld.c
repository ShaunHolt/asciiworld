#include <math.h>
#include <shapefil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

struct screen
{
    int width, height;
    char *data;
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
screen_show_interpreted(struct screen *s)
{
    int x, y;
    char a, b, c, d;

    for (y = 0; y < s->height - 1; y += 2)
    {
        for (x = 0; x < s->width - 1; x += 2)
        {
            a = s->data[y * s->width + x];
            b = s->data[y * s->width + x + 1];
            c = s->data[(y + 1) * s->width + x];
            d = s->data[(y + 1) * s->width + x + 1];

            if (a == 2 || b == 2 || c == 2 || d == 2)
                printf("\033[31mX\033[0m");

            else if (!a && !b && !c && !d)
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
        }
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
    screen_set_pixel(s, x, y, 1);

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
        screen_set_pixel(s, x, y, 1);
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

            s->data[(int)sy * s->width + (int)sx] = 2;
        }
    }

    fclose(fp);

    return 1;
}

int
main(int argc, char **argv)
{
    struct screen s;
    struct winsize w;
    int opt;
    char *map = "ne_110m_land.shp";
    char *highlight_locations = NULL;

    if (isatty(STDOUT_FILENO))
    {
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    }
    else
    {
        w.ws_col = 80;
        w.ws_row = 24;
    }

    while ((opt = getopt(argc, argv, "w:h:m:l:")) != -1)
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
    screen_show_interpreted(&s);

    exit(EXIT_SUCCESS);
}
