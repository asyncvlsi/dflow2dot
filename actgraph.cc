/*************************************************************************
 *
 *  This file is part of the ACT dataflow visualization pass
 *
 *  Copyright (c) 2023 Rajit Manohar
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
#include <stdio.h>
#include <act/act.h>
#include <act/passes.h>
#include <common/hash.h>
#include <common/agraph.h>
#include "actgraph.h"
#include <unordered_map>

struct channel_io {
  list_t *src_list;
  list_t *dst_list;
};


class EdgeInfo : public AGinfo {
 public:
  EdgeInfo (act_connection *c) {
    _src = NULL;
    _dst = NULL;
    _c = c;
    _infobuf = NULL;
  }

  void set_src(act_connection *c) {
    _src = c;
  }
  void set_dst(act_connection *c) {
    _dst = c;
  }
  
  ~EdgeInfo() { if (_infobuf) FREE (_infobuf); }

  const char *info() {
    if (!_infobuf) {
      char buf[1024];
      ActId *x = _c->toid();
      x->sPrint (buf, 1024);
      delete x;
      std::string s = buf;
      if (_src) {
	s = s + "\"; labelfontsize=10; labelfontcolor=blue; taillabel=\"";
	x = _src->toid();
	x->sPrint (buf, 10240);
	delete x;
	s = s + buf;
      }
      if (_dst) {
	if (!_src) {
	  s = s + "\"; labelfontsize=10; labelfontcolor=blue; headlabel=\"";
	}
	else {
	  s = s + "\"; headlabel=\"";
	}
	x = _dst->toid();
	x->sPrint (buf, 10240);
	delete x;
	s = s + buf;
      }
      _infobuf = Strdup (s.c_str());
    }
    return _infobuf;
  }
  
 private:
  char *_infobuf;
  act_connection *_c;
  act_connection *_src, *_dst;
};

class VertexInfo : public AGinfo {
 public:
  VertexInfo (act_connection *c, Process *p = NULL) {
    _infobuf = NULL;
    type = 1;
    o1._c = c;
    o1._p = p;
  }
  VertexInfo (ValueIdx *vx, int off = -1) {
    _infobuf = NULL;
    type = 2;
    o2._vx = vx;
    o2._off = off;
  }
  
  ~VertexInfo() { if (_infobuf) FREE (_infobuf); };

  void set_num (int x) { _num = x; }
  int get_num()  { return _num; }

  const char *info() {
    if (!_infobuf) {
      char buf[10240];
      int sz = 10240;
      int len = 0;
      int pos = 0;

      if (type == 1) {
	ActId *tmp = o1._c->toid();
	tmp->sPrint (buf + pos, sz);
	delete tmp;
	len = strlen (buf+pos); sz -= len; pos += len;

	if (o1._p) {
	  snprintf (buf+ pos, sz, " / ");
	  len = strlen (buf+pos); sz -= len; pos += len;
	  
	  char *tmp = o1._p->getFullName ();
	  snprintf (buf+pos, sz, "%s", tmp);
	  FREE (tmp);
	}
	else {
	  snprintf (buf + pos, sz, "[c]\";shape=box;style=filled;fillcolor=\"yellow");
	}
      }
      else {
	snprintf (buf, sz, "%s / %s", o2._vx->getName(),
		  o2._vx->t->BaseType()->getName());
	len = strlen (buf+pos);  pos += len; sz -= len;
	if (o2._off != -1) {
	  snprintf (buf + pos, sz, " [%d]", o2._off);
	}
      }
      _infobuf = Strdup (buf);
    }
    return _infobuf;
  }

 private:
  char *_infobuf;
  int type;
  struct opt1 {
    act_connection *_c;
    Process *_p;
  };
  struct opt2 {
    ValueIdx *_vx;
    int _off;
  };
  union {
    struct opt1 o1;
    struct opt2 o2;
  };
  int _num;
};

void actgraph_init(ActPass *ap)
{
  ActDynamicPass *dp = dynamic_cast<ActDynamicPass *>(ap);
  ActBooleanizePass *bp;
  Assert(dp, "What?!");
  config_set_state(dp->getConfig());
  if (!ap->getAct()->pass_find ("booleanize")) {
    bp = new ActBooleanizePass (ap->getAct());
  }
  dp->addDependency ("booleanize");
}

void *actgraph_proc (ActPass *ap, Process *p, int mode)
{
  AGraph *g;
  ActDynamicPass *dp;
  ActBooleanizePass *bp;
  int flat_mode;
  int prs_mode;
  
  if (!p) {
    return NULL;
  }

  /* compute subgraph, and return it! */
  bp = dynamic_cast <ActBooleanizePass *> (ap->getAct()->pass_find ("booleanize"));
  Assert (bp, "What?");

  dp = dynamic_cast <ActDynamicPass *> (ap);
  Assert (dp, "What?");

  if (dp->hasParam ("flat")) {
    flat_mode = dp->getIntParam ("flat");
  }
  else {
    flat_mode = 0;
  }
  if (dp->hasParam("prs")) {
    prs_mode = dp->getIntParam ("prs");
  }
  else {
    prs_mode = 0;
  }

  act_boolean_netlist_t *nl = bp->getBNL (p);

  std::unordered_map<act_connection *,
    std::vector<std::tuple<act_connection *,int,int>>> local_nets;
  local_nets.clear ();
  
  
  Assert (nl, "What?");

  g = new AGraph;
  // we should add our own inputs and outputs!
  if (prs_mode) {
    for (int i=0; i < A_LEN (nl->ports); i++) {
      if (nl->ports[i].omit) continue;
      VertexInfo *vi = new VertexInfo (nl->ports[i].c);
      int mode;
      if (nl->ports[i].input && !nl->ports[i].bidir) {
	vi->set_num (g->addInput (vi));
	mode = 0;
      }
      else {
	vi->set_num (g->addOutput (vi));
	if (nl->ports[i].bidir) {
	  mode = 2;
	}
	else {
	  mode = 1;
	}
      }
      local_nets[nl->ports[i].c].push_back(std::tuple<act_connection*,int,int>
					   (NULL,vi->get_num(),mode));
    }
  }
  else {
    for (int i=0; i < A_LEN (nl->chpports); i++) {
      if (nl->chpports[i].omit) continue;
      VertexInfo *vi = new VertexInfo (nl->chpports[i].c);
      int mode;
      if (nl->chpports[i].input && !nl->chpports[i].bidir) {
	vi->set_num (g->addInput (vi));
	mode = 0;
      }
      else {
	vi->set_num (g->addOutput (vi));
	if (nl->ports[i].bidir) {
	  mode = 2;
	}
	else {
	  mode = 1;
	}
      }
      local_nets[nl->chpports[i].c].push_back(std::tuple<act_connection*,int,int>
					      (NULL,vi->get_num(),mode));
    }
  }

  // walk through instances
  ActUniqProcInstiter i(p ? p->CurScope() : ActNamespace::Global()->CurScope());
  int instcount = 0;
  for (i = i.begin(); i != i.end(); i++) {
    ValueIdx *vx = *i;
    Process *x = dynamic_cast<Process *>(vx->t->BaseType());
    act_boolean_netlist_t *subnl;
    Assert (x->isExpanded(), "What?");

    subnl = bp->getBNL (x);
    Assert (subnl, "What?");

    int ports_exist;
    if (prs_mode) {
      for (int j=0; j < A_LEN (subnl->ports); j++) {
	if (subnl->ports[j].omit == 0) {
	  ports_exist = 1;
	  break;
	}
      }
    }
    else {
      for (int j=0; j < A_LEN (subnl->chpports); j++) {
	if (subnl->chpports[j].omit == 0) {
	  ports_exist = 1;
	  break;
	}
      }
    }

    if (ports_exist) {
      int sz;
      std::vector<int> cur_inst;
      cur_inst.clear ();
      if (vx->t->arrayInfo()) {
	int count = 0;
	for (int k=0; k < vx->t->arrayInfo()->size(); k++) {
	  if (vx->isPrimary (k)) {
	    count++;
	  }
	}
	sz = count;
      }
      else {
	sz = 1;
      }

      int loc = 0;

      while (sz > 0) {
	sz--;

	VertexInfo *vi;
	int vtx_id;
	if (!vx->t->arrayInfo()) {
	  vi = new VertexInfo (vx);
	}
	else {
	  while (!vx->isPrimary (loc)) {
	    loc++;
	    Assert (loc < vx->t->arrayInfo()->size(), "Hmm");
	  }
	  vi = new VertexInfo (vx, loc);
	  loc++;
	}
	vtx_id = g->addVertex (vi);
	vi->set_num (vtx_id);
	
	for (int j=0; j < (prs_mode ? A_LEN (subnl->ports)
			   : A_LEN (subnl->chpports)); j++) {
	  act_connection *c, *subc;
	  netlist_bool_port *np;

	  np = prs_mode ? &subnl->ports[j] : &subnl->chpports[j];
	  
	  if (np->omit) {
	    continue;
	  }
	  subc = np->c;
	  c = prs_mode ? nl->instports[instcount] : nl->instchpports[instcount];
	  if (c->isglobal()) {
	    instcount++;
	    continue;
	  }

	  int mode;
	  if (np->bidir) {
	    mode = 2;
	  }
	  else if (np->input) {
	    mode = 0;
	  }
	  else {
	    mode = 1;
	  }

	  /* c is the uniq name for the signal/channel/connection */
	  local_nets[c].push_back (std::tuple<act_connection*,int,int>
				   (subc,vtx_id,mode));

	  instcount++;
	}
      }
    }
  }

  for (auto &[conn, vec] : local_nets) {
    EdgeInfo *ei;
    if (vec.size() == 2) {
      ei = new EdgeInfo (conn);
      auto &[conn0, id0, mode0] = vec[0];
      auto &[conn1, id1, mode1] = vec[1];

      if (mode0 == 0 && mode1 != 0) {
	g->addEdge (id1, id0, ei);
	ei->set_src (conn1);
	ei->set_dst (conn0);
      }
      else if (mode1 == 0 && mode0 != 0) {
	g->addEdge (id0, id1, ei);
	ei->set_src (conn0);
	ei->set_dst (conn1);
      }
      else {
	if (g->getVertex(id0)->isio == 2 || g->getVertex(id1)->isio == 1) {
	  g->addEdge (id1, id0, ei);
	  ei->set_src (conn1);
	  ei->set_dst (conn0);
	}
	else {
	  g->addEdge (id0, id1, ei);
	  ei->set_src (conn0);
	  ei->set_dst (conn1);
	}	  
      }
    }
    else if (vec.size() == 1) {
      // no edge!
    }
    else {
      VertexInfo *vi;
      vi = new VertexInfo (conn);
      int vtx = g->addVertex (vi);
      
      for (auto &[conn, id, mode] : vec) {
	if (mode == 0) {
	  g->addEdge (vtx, id);
	}
	else {
	  g->addEdge (id, vtx);
	}
      }
    }
  }

  FILE *fp;
  char buf[1024];
  int len;
  ap->getAct()->msnprintfproc (buf, 1024, p);
  len = strlen (buf);
  snprintf (buf + len, 1024 - len, ".dot");

  fp = fopen (buf, "w");
  if (!fp) {
    fatal_error ("Could not create file `%s'", buf);
  }
  g->printDot (fp, p->getName());
  fclose (fp);
  return g;
}

void actgraph_free(ActPass *ap, void *v)
{
}

void actgraph_done(ActPass *ap)
{
  ActDynamicPass *dp = dynamic_cast<ActDynamicPass *>(ap);
}
