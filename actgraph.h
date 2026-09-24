/*************************************************************************
 *
 *  This file is part of the ACT gate sizing pass implementation
 *
 *  Copyright (c) 2022 Rajit Manohar
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA  02110-1301, USA.
 *
 **************************************************************************
 */
#ifndef __ACT_ACTGRAPH_PASS_H__
#define __ACT_ACTGRAPH_PASS_H__
#include <vector>
#include <string>

extern "C"
{

  void actgraph_init(ActPass *ap);
  void actgraph_run(ActPass *ap, Process *p);
  void actgraph_recursive(ActPass *ap, Process *p, int mode);
  void *actgraph_proc(ActPass *ap, Process *p, int mode);
  void *actgraph_data(ActPass *ap, Data *d, int mode);
  void *actgraph_chan(ActPass *ap, Channel *c, int mode);
  int actgraph_runcmd(ActPass *ap, const char *name);
  void actgraph_free(ActPass *ap, void *v);
  void actgraph_done(ActPass *ap);
}

#endif /*  __ACT_DFLOWGRAPH_PASS_H__ */
