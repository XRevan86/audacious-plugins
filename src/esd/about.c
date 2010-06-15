/*      xmms - esound output plugin
 *    Copyright (C) 1999      Galex Yen
 *      3/9/99
 *      
 *      this program is free software
 *      
 *      Description:
 *              This program is an output plugin for xmms v0.9 or greater.
 *              The program uses the esound daemon to output audio in order
 *              to allow more than one program to play audio on the same
 *              device at the same time.
 *
 *              Contains code Copyright (C) 1998-1999 Mikael Alm, Olle Hallnas,
 *              Thomas Nillson and 4Front Technologies
 */

#include "esdout.h"

#include <glib.h>
#include <audacious/i18n.h>
#include <gtk/gtk.h>

#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>

void
esdout_about(void)
{
    static GtkWidget *dialog = NULL;

    audgui_simple_message(&dialog, GTK_MESSAGE_INFO, _("About ESounD Plugin"),
                               _("Audacious ESounD Plugin\n\n "
                                 "This program is free software; you can redistribute it and/or modify\n"
                                 "it under the terms of the GNU General Public License as published by\n"
                                 "the Free Software Foundation; either version 2 of the License, or\n"
                                 "(at your option) any later version.\n"
                                 "\n"
                                 "This program is distributed in the hope that it will be useful,\n"
                                 "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
                                 "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
                                 "GNU General Public License for more details.\n"
                                 "\n"
                                 "You should have received a copy of the GNU General Public License\n"
                                 "along with this program; if not, write to the Free Software\n"
                                 "Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,\n"
                                 "USA."));
}
