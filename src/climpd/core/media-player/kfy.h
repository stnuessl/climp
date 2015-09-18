/*
 * Copyright (C) 2015  Steffen Nüssle
 * climp - Command Line Interface Music Player
 *
 * This file is part of climp.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _KFY_H_
#define _KFY_H_

#include <stdbool.h>

void kfy_init(unsigned int size);

void kfy_destroy(void);

void kfy_reset(void);

unsigned int kfy_shuffle(void);

int kfy_add(unsigned int cnt);

void kfy_remove(unsigned int cnt);

unsigned int kfy_size(void);

bool kfy_cycle_done(void);

#endif /* _KFY_H_ */