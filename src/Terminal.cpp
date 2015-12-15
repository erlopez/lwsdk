/********************************************************************************
 *  This file is part of the lopezworks SDK utility library for C/C++ (lwsdk).
 *
 *  Copyright (C) 2015-2017 Edwin R. Lopez
 *  http://www.lopezworks.info
 *
 *  This source code is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation LGPL v2.1
 *  (http://www.gnu.org/licenses/).
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 *  02110-1301  USA.
 ********************************************************************************/
#include "Terminal.h"

#include <ctime>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <termios.h>

namespace lwsdk::Terminal
{

    /**
     * Enable of disable terminal Canonical mode. Wneh isEnabled=1 it removes the requirement of
     * hitting CR/RETURN to make getchar() return.
     */
    static void nonblock( bool isEnabled )
    {
        struct termios ttystate{};

        //get the terminal state
        tcgetattr( STDIN_FILENO, &ttystate );

        if ( isEnabled )
        {
            //turn off canonical mode
            ttystate.c_lflag &= ~ICANON;     // disable line buffering
            ttystate.c_cc[VMIN] = 1;         // minimum of number input read.
            setvbuf(stdin, NULL, _IONBF, 0); // turn off buffering
        }
        else
        {
            //turn on canonical mode
            ttystate.c_lflag |= ICANON;      // restore line buffering
            setvbuf(stdin, NULL, _IOLBF, 0); // restore stdin buffering

        }
        //set the terminal attributes.
        tcsetattr( STDIN_FILENO, TCSANOW, &ttystate );

    }

    static void reset()
    {
        nonblock( false );
    }


    bool kbhit()
    {
        static int inited = 0;
        if ( !inited )
        {
            nonblock( true );
            inited = 1;
            atexit( reset );
        }

        struct timeval tv{};
        fd_set fds;
        tv.tv_sec = 0;
        tv.tv_usec = 0;
        FD_ZERO( &fds );
        FD_SET( STDIN_FILENO, &fds ); //STDIN_FILENO is 0

        select( STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv );

        return FD_ISSET( STDIN_FILENO, &fds ) != 0;
    }

} //ns