/*
 * Copyright (c) 2017-2019 Marc Sanchez Fauste.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <math.h>
#include <QElapsedTimer>

#include <libaudcore/i18n.h>
#include <libaudcore/interface.h>
#include <libaudcore/runtime.h>
#include <stdio.h>
#include <string.h>

#include "utils.h"
#include "vumeter_qt_widget.h"
#include "vumeter_qt.h"

#define MAX_CHANNELS 20
#define DB_RANGE 96

EXPORT VUMeterQt aud_plugin_instance;

const char VUMeterQt::about[] =
 N_("VU Meter Plugin for Audacious\n"
    "Copyright 2017-2019 Marc Sánchez Fauste");

const PreferencesWidget VUMeterQt::widgets[] = {
    WidgetLabel (N_("<b>VU Meter Settings</b>")),
    WidgetSpin (
        N_("Peak hold time:"),
        WidgetFloat ("vumeter", "peak_hold_time"),
        {0.1, 30, 0.1, N_("seconds")}
    ),
    WidgetSpin (
        N_("Fall-off time:"),
        WidgetFloat ("vumeter", "falloff"),
        {0.1, 96, 0.1, N_("dB/second")}
    ),
    WidgetCheck (N_("Display legend"),
        WidgetBool ("vumeter", "display_legend", VUMeterQtWidget::toggle_display_legend)),
};

const PluginPreferences VUMeterQt::prefs = {{widgets}};

const char * const VUMeterQt::prefs_defaults[] = {
    "peak_hold_time", "1.6",
    "falloff", "13.3",
    "display_legend", "TRUE",
    nullptr
};

static VUMeterQtWidget * spect_widget = nullptr;
static int nchannels = 2;
static float channels_db_level[MAX_CHANNELS];
static float channels_peaks[MAX_CHANNELS];
static QElapsedTimer last_peak_times[MAX_CHANNELS]; // Time elapsed since peak was set
QElapsedTimer render_timer;

float VUMeterQt::get_db_on_range(float db)
{
    return fclamp(db, -DB_RANGE, 0);
}

float VUMeterQtWidget::get_db_factor(float db)
{
    float factor = 0.0f;

    if (db < -DB_RANGE) {
        factor = 0.0f;
    } else if (db < -60.0f) {
        factor = (db + DB_RANGE) * 2.5f/(DB_RANGE-60);
    } else if (db < -50.0f) {
        factor = (db + 60.0f) * 0.5f + 2.5f;
    } else if (db < -40.0f) {
        factor = (db + 50.0f) * 0.75f + 7.5f;
    } else if (db < -30.0f) {
        factor = (db + 40.0f) * 1.5f + 15.0f;
    } else if (db < -20.0f) {
        factor = (db + 30.0f) * 2.0f + 30.0f;
    } else if (db < 0.0f) {
        factor = (db + 20.0f) * 2.5f + 50.0f;
    } else {
        factor = 100.0f;
    }

    return factor / 100.0f;
}

float VUMeterQtWidget::get_height_from_db(float db)
{
    return get_db_factor(db) * vumeter_height;
}

float VUMeterQtWidget::get_y_from_db(float db)
{
    return vumeter_top_padding + vumeter_height - get_height_from_db(db);
}

void VUMeterQt::render_multi_pcm (const float * pcm, int channels)
{
    qint64 elapsed_render_time = render_timer.restart();
    nchannels = fclamp(channels, 0, MAX_CHANNELS);
    float falloff = aud_get_double ("vumeter", "falloff") / 1000.0;
    qint64 peak_hold_time = aud_get_double ("vumeter", "peak_hold_time") * 1000;

    float peaks[channels];
    for (int channel = 0; channel < channels; channel++)
    {
        peaks[channel] = fabsf(pcm[channel]);
    }

    for (int i = 0; i < 512 * channels;)
    {
        for (int channel = 0; channel < channels; channel++)
        {
            peaks[channel] = fmaxf(peaks[channel], fabsf(pcm[i++]));
        }
    }

    for (int i = 0; i < nchannels; i++)
    {
        float n = peaks[i];

        float db = 20 * log10f(n);
        db = get_db_on_range(db);

        channels_db_level[i] = get_db_on_range(channels_db_level[i] - elapsed_render_time * falloff);

        if (db > channels_db_level[i])
        {
            channels_db_level[i] = db;
        }

        qint64 elapsed_peak_time = last_peak_times[i].elapsed();
        if (channels_db_level[i] > channels_peaks[i] || elapsed_peak_time > peak_hold_time) {
            channels_peaks[i] = channels_db_level[i];
            last_peak_times[i].restart();
        }

    }

    if (spect_widget) {
        spect_widget->update();
    }
}

bool VUMeterQt::init ()
{
    render_timer.start();
    for (int i = 0; i < MAX_CHANNELS; i++) {
        last_peak_times[i].start();
        channels_db_level[i] = -DB_RANGE;
        channels_peaks[i] = -DB_RANGE;
    }

    aud_config_set_defaults ("vumeter", prefs_defaults);
    return true;
}

void VUMeterQt::clear ()
{
    render_timer.restart();
    for (int i = 0; i < MAX_CHANNELS; i++) {
        last_peak_times[i].restart();
        channels_db_level[i] = -DB_RANGE;
        channels_peaks[i] = -DB_RANGE;
    }

    if (spect_widget) {
        spect_widget->update();
    }
}

void VUMeterQtWidget::draw_background(QPainter & p)
{
    p.fillRect(0, 0, width(), height(), QColor(40, 40, 40, 255));
}

void VUMeterQtWidget::draw_vu_legend(QPainter & p)
{
    float font_size_width = legend_width / 4.0f;
    float font_size_height = vumeter_height * 0.015f;

    QFont font = p.font();
    font.setPointSizeF(fminf(font_size_width, font_size_height));
    p.setFont(font);

    QPen pen;
    pen.setWidth(1);
    pen.setColor(QColor(255, 255, 255));
    p.setPen(pen);

    draw_vu_legend_db(p, 0, "0");
    draw_vu_legend_db(p, -3, "-3");
    draw_vu_legend_db(p, -6, "-6");
    draw_vu_legend_db(p, -9, "-9");
    draw_vu_legend_db(p, -12, "-12");
    draw_vu_legend_db(p, -15, "-15");
    draw_vu_legend_db(p, -18, "-18");
    draw_vu_legend_db(p, -20, "-20");
    draw_vu_legend_db(p, -25, "-25");
    draw_vu_legend_db(p, -30, "-30");
    draw_vu_legend_db(p, -35, "-35");
    draw_vu_legend_db(p, -40, "-40");
    draw_vu_legend_db(p, -50, "-50");
    draw_vu_legend_db(p, -60, "-60");
    draw_vu_legend_db(p, -DB_RANGE, "-inf");

    pen.setColor(QColor(120, 120, 120));
    p.setPen(pen);
    for (int i = 0; i > -DB_RANGE; i--)
    {
        if (i > -30)
        {
            draw_vu_legend_line(p, i);
            draw_vu_legend_line(p, i - 0.5, 0.5);
        }
        else if (i > -40)
        {
            draw_vu_legend_line(p, i);
        }
        else if (i > -60)
        {
            draw_vu_legend_line(p, i);
            i -= 1;
        }
        else
        {
            draw_vu_legend_line(p, i);
            i -= (DB_RANGE - 60) / 2;
        }
    }
    draw_vu_legend_line(p, -DB_RANGE);
}

void VUMeterQtWidget::draw_vu_legend_line(QPainter &p, float db, float line_width_factor)
{
    float y = get_y_from_db(db);
    float line_width = fclamp(legend_width * 0.25f, 1, 8);
    p.drawLine(QPointF(legend_width - line_width * line_width_factor, y), QPointF(legend_width, y));
    p.drawLine(QPointF(width() - legend_width, y), QPointF(width() - legend_width + line_width * line_width_factor, y));
}

void VUMeterQtWidget::draw_vu_legend_db(QPainter &p, float db, const char *text)
{
    QFontMetricsF fm(p.font());
    QSizeF text_size = fm.size(0, text);
    float y = get_y_from_db(db);
    float padding = fclamp(legend_width * 0.25f, 1, 8) * 1.5f;
    p.drawText(QPointF(legend_width - text_size.width() - padding, y + (text_size.height()/4.0f)), text);
    p.drawText(QPointF(width() - legend_width + padding, y + (text_size.height()/4.0f)), text);
}

void VUMeterQtWidget::draw_visualizer_peaks(QPainter &p)
{
    float bar_width = get_bar_width(nchannels);
    float font_size_width = bar_width / 3.0f;
    float font_size_height = vumeter_top_padding * 0.50f;

    QFont font = p.font();
    font.setPointSizeF(fminf(font_size_width, font_size_height));
    p.setFont(font);

    QPen pen;
    pen.setColor(QColor(255, 255, 255));
    p.setPen(pen);

    QFontMetricsF fm(p.font());
    char text[10];
    for (int i = 0; i < nchannels; i++)
    {
        format_db(text, channels_peaks[i]);
        QSizeF text_size = fm.size(0, text);
        p.drawText(QPointF(legend_width + bar_width*(i+0.5f) - text_size.width()/2.0f,
            vumeter_top_padding/2.0f + (text_size.height()/4.0f)), text
        );
    }
}

void VUMeterQtWidget::draw_visualizer(QPainter & p)
{
    for (int i = 0; i < nchannels; i++)
    {
        float bar_width = get_bar_width(nchannels);
        float x = legend_width + (bar_width * i);
        if (i > 0)
        {
             x += 1;
        }

        p.fillRect (
            QRectF(x, vumeter_top_padding, bar_width, vumeter_height),
            background_vumeter_pattern
        );

        p.fillRect (
            QRectF(x, get_y_from_db(channels_db_level[i]),
                bar_width, (get_height_from_db(channels_db_level[i]))),
            vumeter_pattern
        );

        if (channels_peaks[i] > -DB_RANGE)
        {
            p.fillRect (
                QRectF(x, get_y_from_db(channels_peaks[i]), bar_width, 1),
                vumeter_pattern
            );
        }
    }
}

void VUMeterQtWidget::format_db(char *buf, const float val) {
    if (val > -10)
    {
        sprintf(buf, "%+.1f", val);
    }
    else if (val > -DB_RANGE)
    {
        sprintf(buf, "%.0f ", val);
    }
    else
    {
        sprintf(buf, "-inf");
    }
}

float VUMeterQtWidget::get_bar_width(int channels)
{
    return vumeter_width / channels;
}

void VUMeterQtWidget::update_sizes()
{
    if (height() > 200 && width() > 60 && aud_get_bool("vumeter", "display_legend")) {
        must_draw_vu_legend = true;
        vumeter_top_padding = height() * 0.03f;
        vumeter_bottom_padding = height() * 0.015f;
        vumeter_height = height() - vumeter_top_padding - vumeter_bottom_padding;
        legend_width = width() * 0.3f;
        vumeter_width = width() - (legend_width * 2);
    } else {
        must_draw_vu_legend = false;
        vumeter_top_padding = 0;
        vumeter_bottom_padding = 0;
        vumeter_height = height();
        legend_width = 0;
        vumeter_width = width();
    }
    vumeter_pattern = get_vumeter_pattern();
    background_vumeter_pattern = get_vumeter_pattern(25);
}

VUMeterQtWidget::VUMeterQtWidget (QWidget * parent) : QWidget (parent)
{
    update_sizes();

    setObjectName ("VUMeterQtWidget");
}

QLinearGradient VUMeterQtWidget::get_vumeter_pattern(int alpha)
{
    QLinearGradient vumeter_pattern = QLinearGradient(
        0, vumeter_top_padding + vumeter_height, 0, vumeter_top_padding
    );
    vumeter_pattern.setColorAt(get_db_factor(0), QColor(190, 40, 10, alpha));
    vumeter_pattern.setColorAt(get_db_factor(-2), QColor(190, 40, 10, alpha));
    vumeter_pattern.setColorAt(get_db_factor(-9), QColor(210, 210, 15, alpha));
    vumeter_pattern.setColorAt(get_db_factor(-50), QColor(0, 190, 20, alpha));
    return vumeter_pattern;
}

VUMeterQtWidget::~VUMeterQtWidget ()
{
    spect_widget = nullptr;
}

void VUMeterQtWidget::resizeEvent (QResizeEvent * event)
{
    update_sizes();
    update ();
}

void VUMeterQtWidget::paintEvent (QPaintEvent * event)
{
    QPainter p(this);

    draw_background(p);
    if (must_draw_vu_legend) {
        draw_vu_legend(p);
        draw_visualizer_peaks(p);
    }
    draw_visualizer(p);
}

void * VUMeterQt::get_qt_widget ()
{
    if (spect_widget) {
        return spect_widget;
    }

    spect_widget = new VUMeterQtWidget;
    return spect_widget;
}

void VUMeterQtWidget::toggle_display_legend()
{
    if (spect_widget)
    {
        spect_widget->update_sizes();
    }
}
