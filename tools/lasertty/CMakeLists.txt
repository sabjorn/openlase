#         OpenLase - a realtime laser graphics toolkit
#
# Copyright (C) 2009-2011 Hector Martin "marcan" <hector@marcansoft.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation, either version 2.1 or version 3.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
#

include_directories(${CMAKE_SOURCE_DIR}/include ${JACK_INCLUDE_DIR})
link_directories(${CMAKE_BINARY_DIR}/libol)

add_executable(lasertty
    lasertty.c
    pty.c
    termbuf.c
    input.c
)

target_link_libraries(lasertty
    ol
    ${JACK_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    util
)
