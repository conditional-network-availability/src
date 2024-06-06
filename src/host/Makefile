################################################################################
#  Copyright (C) 2022-2024 Jonas Röckl <joroec@gmx.net>
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; If not, see <http://www.gnu.org/licenses/>.
################################################################################

all: sender receiver nwreceiver nwsender latency nwlatency echo nwecho

sender: sender.c
	gcc -Wall sender.c -o sender

receiver: receiver.c
	gcc -Wall receiver.c -o receiver -lm

nwreceiver: nwreceiver.c
	gcc -Wall nwreceiver.c -o nwreceiver -lm

nwsender: nwsender.c
	gcc -Wall nwsender.c -o nwsender

latency: latency.c
	gcc -Wall latency.c -o latency -lm

echo: echo.c
	gcc -Wall echo.c -o echo -lm

nwecho: nwecho.c
	gcc -Wall nwecho.c -o nwecho -lm

nwlatency: nwlatency.c
	gcc -Wall nwlatency.c -o nwlatency -lm
