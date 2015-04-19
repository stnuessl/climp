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

#include <libvci/random.h>

struct kfy {
    struct random rand;
    unsigned int *a;
    unsigned int end;
    unsigned int size;
    unsigned int capacity;
};

int kfy_init(struct kfy *__restrict k, unsigned int size);

void kfy_destroy(struct kfy *__restrict k);

void kfy_reset(struct kfy *__restrict k);

unsigned int kfy_shuffle(struct kfy *__restrict k);

int kfy_add(struct kfy *__restrict k, unsigned int cnt);

void kfy_remove(struct kfy *__restrict k, unsigned int cnt);

unsigned int kfy_size(const struct kfy *__restrict k);

bool kfy_cycle_done(const struct kfy *__restrict k);

#endif /* _KFY_H_ */