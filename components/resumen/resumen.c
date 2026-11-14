/* resumen.c - resumen del periodo de una actividad periódica. Ver resumen.h. */
#include "resumen.h"

#include <inttypes.h>
#include <stdio.h>

static void empezar(resumen_t *r)
{
    r->n = 0;
    r->suma_us = 0;
    r->min_us = INT64_MAX;
    r->max_us = 0;
    r->rebases = 0;
}

void resumen_iniciar(resumen_t *r, uint32_t objetivo)
{
    r->objetivo = objetivo > 0 ? objetivo : 1;
    resumen_reiniciar(r);
}

void resumen_reiniciar(resumen_t *r)
{
    empezar(r);
    r->anterior_us = -1;
}

bool resumen_anotar(resumen_t *r, int64_t instante_us, bool tardia, resumen_datos_t *datos)
{
    if (r->anterior_us < 0)
    {
        /* La primera activación solo marca el comienzo. */
        r->anterior_us = instante_us;
        return false;
    }

    const int64_t intervalo = instante_us - r->anterior_us;
    r->anterior_us = instante_us;
    r->n++;
    r->suma_us += intervalo;
    if (intervalo < r->min_us)
    {
        r->min_us = intervalo;
    }
    if (intervalo > r->max_us)
    {
        r->max_us = intervalo;
    }
    if (tardia)
    {
        r->rebases++;
    }

    if (r->n < r->objetivo)
    {
        return false;
    }

    datos->n = r->n;
    datos->medio_us = r->suma_us / r->n;
    datos->min_us = r->min_us;
    datos->max_us = r->max_us;
    datos->jitter_us = r->max_us - r->min_us;
    datos->rebases = r->rebases;
    /* El siguiente resumen empieza en esta activación: no se pierde ninguna. */
    empezar(r);
    return true;
}

uint32_t resumen_llevados(const resumen_t *r)
{
    return r->n;
}

/* Milisegundos con tres decimales y coma decimal, por ejemplo 1000,012. */
static void escribir_ms(char *destino, size_t tam, int64_t us)
{
    const char *signo = us < 0 ? "-" : "";
    if (us < 0)
    {
        us = -us;
    }
    snprintf(destino, tam, "%s%" PRId64 ",%03" PRId64, signo, us / 1000, us % 1000);
}

void resumen_escribir(const char *etiqueta, const resumen_datos_t *d, int64_t nominal_us)
{
    char medio[24], minimo[24], maximo[24], jitter[24], nominal[24];
    escribir_ms(medio, sizeof medio, d->medio_us);
    escribir_ms(minimo, sizeof minimo, d->min_us);
    escribir_ms(maximo, sizeof maximo, d->max_us);
    escribir_ms(jitter, sizeof jitter, d->jitter_us);
    escribir_ms(nominal, sizeof nominal, nominal_us);
    printf("%s: %" PRIu32 " intervalos; medio %s ms (nominal %s), min %s, max %s, "
           "jitter %s ms, rebases %" PRIu32 "\n",
           etiqueta, d->n, medio, nominal, minimo, maximo, jitter, d->rebases);
}
