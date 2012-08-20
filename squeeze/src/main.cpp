/***************************************************************************
 *   Copyright © 2007-2010 by Miguel Chavez Gamboa                         *
 *   miguel@lemonpos.org                                                   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.         *
 ***************************************************************************/


#include "squeeze.h"
#include <kapplication.h>
#include <kaboutdata.h>
#include <kcmdlineargs.h>
#include <klocale.h>

static const char description[] =
    I18N_NOOP("Squeeze your lemon!");

static const char version[] = "1.0 Preview / 2010.Jan.02";

int main(int argc, char **argv)
{
  KAboutData about("squeeze", 0, ki18n("squeeze"), version, ki18n(description), KAboutData::License_GPL, ki18n("© 2007-2010 Miguel Chavez Gamboa"), KLocalizedString(), 0, "miguel@lemonpos.org           ");
    about.addAuthor( ki18n("Miguel Chávez Gamboa"), KLocalizedString(), "miguel@lemonpos.org           " );
    KCmdLineArgs::init(argc, argv, &about);

    about.addCredit(ki18n("Roberto Aceves"), ki18n("Many ideas and general help"));
    about.addCredit(ki18n("Biel Frontera"), ki18n("Code contributor"));
    about.addCredit(ki18n("Jose Nivar"), ki18n("Many ideas and sponsorship"));
    
    KCmdLineOptions options;
    options.add("+[URL]", ki18n( "Document to open" ));
    KCmdLineArgs::addCmdLineOptions(options);
    KApplication app;

    // register ourselves as a dcop client
    //app.dcopClient()->registerAs(app.name(), false);

    // see if we are starting with session management
    if (app.isSessionRestored())
        RESTORE(squeeze)
    else
    {
        // no session.. just start up normally
        KCmdLineArgs *args = KCmdLineArgs::parsedArgs();
        if (args->count() == 0)
        {
            squeeze *widget = new squeeze;
            widget->show();
        }
        else
        {
            int i = 0;
            for (; i < args->count(); i++)
            {
                squeeze *widget = new squeeze;
                widget->show();
            }
        }
        args->clear();
    }

    KGlobal::locale()->setCurrencySymbol("€");

    return app.exec();
}
