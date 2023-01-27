/*************************************************************************
 *
 *  This file is part of the ACT gate sizing pass implementation
 *
 *  Copyright (c) 2022 Rajit Manohar, Lucas Nel
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
#include <common/hash.h>
#include <common/agraph.h>
#include "dflowgraph.h"

struct channel_io {
  list_t *src_list;
  list_t *dst_list;
};


class EdgeInfo : public AGinfo {
 public:
  EdgeInfo (const char *nm, int src_port, int dst_port) {
    _nm = Strdup (nm);
    _src = src_port;
    _dst = dst_port;
  }
  ~EdgeInfo() { FREE (_nm); }


  // -2 = n
  // -3 = s
  // -4 = e
  // -5 = w
  int srcPort() { return _src; }
  int dstPort() { return _dst; }

  const char *info() {
    static char buf[1024];
    snprintf (buf, 1024, "%s", _nm);
    return buf;
  }
  
 private:
  char *_nm;
  int _src, _dst;
};

class VertexInfo : public AGinfo {
 public:
  VertexInfo (act_dataflow_element_types t, int num = 0) {
    _t = t;
    _num = num;
    _e = NULL;
    _init = NULL;
  }
  VertexInfo (act_dataflow_element_types t, Expr *e, Expr *init) {
    _t = t;
    _num = 0;
    _e = e;
    _init = init;
  }
  ~VertexInfo() { };

  act_dataflow_element_types getType() { return _t; }

#define BUFFER_SIZE 1024
  
  const char *info() {
    static char buf[BUFFER_SIZE];
    int x;
    switch (_t) {
    case ACT_DFLOW_FUNC:
      snprintf (buf, BUFFER_SIZE, "shape=invhouse;style=filled;fillcolor=antiquewhite;fontsize=\"8pt\";label=\"");
      x = strlen (buf);
      if (!_e) {
	snprintf (buf + x, BUFFER_SIZE - x, "func\"");
	x += strlen (buf+x);
      }
      else {
#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif	
	sprint_uexpr (buf+x, MIN(BUFFER_SIZE - x, 15), _e);
	x += strlen (buf+x);
	if (_init) {
	  snprintf (buf+x, BUFFER_SIZE-x, " {");
	  x += strlen (buf+x);
	  sprint_uexpr (buf+x, BUFFER_SIZE-x, _init);
	  x += strlen (buf+x);
	  snprintf (buf+x, BUFFER_SIZE-x, "}");
	  x += strlen (buf+x);
	}
	snprintf (buf+x, BUFFER_SIZE-x, "\"");
      }
      break;

    case ACT_DFLOW_SPLIT:
      snprintf (buf, BUFFER_SIZE, "shape=trapezium;style=filled;fillcolor=cadetblue1;label=< <table border='0' cellpadding='2' cellborder='0'><tr>");
      x = strlen (buf);
      for (int i=0; i < _num; i++) {
	snprintf (buf+x, BUFFER_SIZE-x, "<td port='p%d'>&nbsp;%d&nbsp;</td>",
		  i, i);
	x += strlen (buf+x);
      }
      snprintf (buf+x, BUFFER_SIZE-x, "</tr></table> >");
      break;

    case ACT_DFLOW_MERGE:
    case ACT_DFLOW_MIXER:
    case ACT_DFLOW_ARBITER:
      snprintf (buf, BUFFER_SIZE, "style=filled;fillcolor=goldenrod;shape=invtrapezium;label=< <table border='0' cellpadding='2' cellborder='0'><tr>");
      x = strlen (buf);
      for (int i=0; i < _num; i++) {
	snprintf (buf+x, BUFFER_SIZE-x, "<td port='p%d'>&nbsp;%d&nbsp;</td>",
		  i, i);
	x += strlen (buf+x);
      }
      snprintf (buf+x, BUFFER_SIZE-x, "</tr></table> >");
      
      break;

    case ACT_DFLOW_SINK:
      snprintf (buf, BUFFER_SIZE, "shape=diamond");
      break;

    default:
      fatal_error ("Unknown type");
      break;
    }
    return buf;
  }

  private:
    act_dataflow_element_types _t;
    int _num;
    Expr *_e, *_init;
};

void dflowgraph_init(ActPass *ap)
{
  ActDynamicPass *dp = dynamic_cast<ActDynamicPass *>(ap);
  Assert(dp, "What?!");
  config_set_state(dp->getConfig());
}

static void _visit_var (struct pHashtable *H, Scope *sc, ActId *id,
			int src = -1, int src_port = -1, 
			int dst = -1, int dst_port = -1)
{
  phash_bucket_t *b;
  channel_io *ch;
  if (!id) return;
  act_connection *c = id->Canonical (sc);
  if (!(b = phash_lookup (H, c))) {
    b = phash_add (H, c);
    NEW (ch, channel_io);
    ch->src_list = list_new ();
    ch->dst_list = list_new ();
    b->v = ch;
  }
  ch = (channel_io *)b->v;
  if (src != -1) {
    list_iappend (ch->src_list, src);
    list_iappend (ch->src_list, src_port);
  }
  if (dst != -1) {
    list_iappend (ch->dst_list, dst);
    list_iappend (ch->dst_list, dst_port);
  }
  return;
}

static void _visit_expr (struct pHashtable *H, Scope *sc, Expr *e,
			 int dst = -1)
{
  if (!e) return;

  // all the fun expressions!

  switch (e->type) {
  case E_PROBE:
    break;
    
  case E_NOT:
  case E_COMPLEMENT:
  case E_UMINUS:
    _visit_expr (H, sc, e->u.e.l, dst);
    break;

  case E_MULT:
  case E_DIV:
  case E_MOD:
  case E_PLUS:
  case E_MINUS:
  case E_LSL:
  case E_LSR:
  case E_ASR:
  case E_LT:
  case E_GT:
  case E_LE:
  case E_GE:
  case E_EQ:
  case E_NE:
  case E_AND:
  case E_XOR:
  case E_OR:
    _visit_expr (H, sc, e->u.e.l, dst);
    _visit_expr (H, sc, e->u.e.r, dst);
    break;

  case E_QUERY: 
    _visit_expr (H, sc, e->u.e.l, dst);
    _visit_expr (H, sc, e->u.e.r->u.e.l, dst);
    _visit_expr (H, sc, e->u.e.r->u.e.r, dst);
    break;

  case E_INT:
  case E_REAL:
  case E_TRUE:
  case E_FALSE:
    break;
    
  case E_VAR:
    _visit_var (H, sc, (ActId *)e->u.e.l, -1, -1, dst, -1);
    break;

  case E_ARRAY:
  case E_SUBRANGE:
    break;

  case E_SELF:
    break;

  case E_SELF_ACK:
    break;

  case E_TYPE:
    break;

  case E_BUILTIN_BOOL:
  case E_BUILTIN_INT:
    _visit_expr (H, sc, e->u.e.l, dst);
    break;

  case E_FUNCTION:
    {
      UserDef *u = (UserDef *)e->u.fn.s;
      Function *f = dynamic_cast<Function *>(u);
      Expr *tmp;
      Assert (f, "Hmm.");
      if (e->u.fn.r && e->u.fn.r->type == E_GT) {
	tmp = e->u.fn.r->u.e.r;
      }
      else {
	tmp = e->u.fn.r;
      }
      while (tmp) {
	_visit_expr (H, sc, tmp->u.e.l, dst);
	tmp = tmp->u.e.r;
      }
    }
    break;
    
  case E_BITFIELD:
    _visit_var (H, sc, (ActId *)e->u.e.l, -1, -1, dst, -1);
    break;

  case E_CONCAT:
    while (e) {
      _visit_expr (H, sc, e->u.e.l, dst);
      e = e->u.e.r;
    }
    break;
    
  default:
    fatal_error ("Unhandled case!\n");
    break;
  }
}


static void _collect_channels (AGraph *ag,
			       struct pHashtable *H, Scope *sc, list_t *df)
{
  listitem_t *li;
  act_dataflow_element *e;
  for (li = list_first (df); li; li = list_next (li)) {
    e = (act_dataflow_element *) list_value (li);
    int vtx;
    int src, dst;
    
    switch (e->t) {
    case ACT_DFLOW_FUNC:
      vtx = ag->addVertex (new VertexInfo (e->t, e->u.func.lhs, e->u.func.init));
      _visit_expr (H, sc, e->u.func.lhs, vtx);
      _visit_var (H, sc, e->u.func.rhs, vtx, -1, -1, -1);
      break;

    case ACT_DFLOW_SPLIT:
    case ACT_DFLOW_MERGE:
    case ACT_DFLOW_MIXER:
    case ACT_DFLOW_ARBITER:
      vtx = ag->addVertex (new VertexInfo (e->t, e->u.splitmerge.nmulti));
      _visit_var (H, sc, e->u.splitmerge.guard, -1, -1, vtx, -5);
      if (e->t == ACT_DFLOW_SPLIT) {
	src = -1;
	dst = vtx;
      }
      else {
	dst = -1;
	src = vtx;
      }
      _visit_var (H, sc, e->u.splitmerge.single, src, -1, dst, -1);
      _visit_var (H, sc, e->u.splitmerge.nondetctrl, vtx, -1, -1, -1);
      for (int i=0; i < e->u.splitmerge.nmulti; i++) {
	if (e->u.splitmerge.multi[i]) {
	  _visit_var (H, sc, e->u.splitmerge.multi[i], dst, i, src, i);
	}
      }
      break;

    case ACT_DFLOW_SINK:
      vtx = ag->addVertex (new VertexInfo (e->t));
      _visit_var (H, sc, e->u.sink.chan, -1, -1, vtx, -1);
      break;

    case ACT_DFLOW_CLUSTER:
      _collect_channels (ag, H, sc, e->u.dflow_cluster);
      break;
      
    default:
      fatal_error ("Unknown type %d!", e->t);
      break;
    }
  }
}


static void printDOT (AGraph *ag, FILE *fp, const char *name)
{
 AGvertex *v;
 AGedge *e;
 const char *t;
 
 fprintf (fp, "digraph \"%s\" {\n", name ? name : "default_name");

 for (int i=0; i < ag->numVertices(); i++) {
   v = ag->getVertex (i);
   fprintf (fp, " v%d ", i);
   if (v->info) {
     t = v->info->info();
     fprintf (fp, " [%s];\n", t);
   }
   else {
     fprintf (fp, " [label=%s;style=filled;fillcolor=%s]",
	      (v->isio == 0 ? "?" : (v->isio == 1 ? "I" : "O")),
	      (v->isio == 0 ? "white" : (v->isio == 1 ? "yellow" : "green")));
   }
 }

 for (int i=0; i < ag->numEdges(); i++) {
   EdgeInfo *ei;
   VertexInfo *vi;
   AGvertex *vx;
   e = ag->getEdge (i);
   if (e->info) {
     ei = dynamic_cast<EdgeInfo *>(e->info);
     t = ei->info();
   }
   else {
     ei = NULL;
     t = "";
   }
   if (ei) {
     int port = ei->srcPort();
     vx = ag->getVertex (e->src);
     if (port == -1 && vx->info) {
       vi = dynamic_cast<VertexInfo *>(vx->info);
       Assert (vi, "Hmm");
       if (vi->getType() == ACT_DFLOW_MERGE) {
	 port = -3;
       }
     }
     fprintf (fp, " v%d", e->src);
     if (port >= 0) {
       fprintf (fp, ":p%d ", port);
     }
     else if (port == -2) {
       fprintf (fp, ":n");
     }
     else if (port == -3) {
       fprintf (fp, ":s");
     }
     else if (port == -4) {
       fprintf (fp, ":e");
     }
     else if (port == -5) {
       fprintf (fp, ":w");
     }
     fprintf (fp, " -> v%d", e->dst);
     port = ei->dstPort();
     vx = ag->getVertex (e->dst);
     if (port == -1 && vx->info) {
       vi = dynamic_cast<VertexInfo *>(vx->info);
       Assert (vi, "Hmm");
       if (vi->getType() == ACT_DFLOW_SPLIT) {
	 port = -2;
       }
     }
     
     if (port >= 0) {
       fprintf (fp, ":p%d ", port);
     }
     else if (port == -2) {
       fprintf (fp, ":n");
     }
     else if (port == -3) {
       fprintf (fp, ":s");
     }
     else if (port == -4) {
       fprintf (fp, ":e");
     }
     else if (port == -5) {
       fprintf (fp, ":w");
     }
     fprintf (fp, " ");
   }
   else {
     fprintf (fp, " v%d -> v%d ", e->src, e->dst);
   }
   fprintf (fp, "[label=\"%s\"];\n", t);
 }
 fprintf (fp, "}\n");
}

void *dflowgraph_proc (ActPass *ap, Process *p, int mode)
{
  if (!p)
  {
    return NULL;
  }

  if (p->getlang() && p->getlang()->getdflow())
  {
    /* process dataflow graph */
    
    ActDynamicPass *dp = dynamic_cast<ActDynamicPass *>(ap);
    struct pHashtable *H = phash_new (4);
    phash_iter_t it;
    phash_bucket_t *b;
    act_dataflow *df = p->getlang()->getdflow();

    AGraph *ag = new AGraph();

    _collect_channels (ag, H, p->CurScope(), p->getlang()->getdflow()->dflow);

    phash_iter_init (H, &it);
    while ((b = phash_iter_next (H, &it))) {
      channel_io *ch = (channel_io *) b->v;
      char buf[1024];
      ActId *tmp;

      tmp = ((act_connection *)b->key)->toid();
      tmp->sPrint (buf, 1024);

      if (list_length (ch->src_list)/2 > 1) {
	warning ("Channel `%s' has more than one driver?", buf);
      }
      if (list_length (ch->src_list) == 0 && list_length (ch->dst_list) != 0) {
	int vtx = ag->addVertex ();
	ag->mkInput (vtx);
	list_iappend (ch->src_list, vtx);
	list_iappend (ch->src_list, -1);
      }
      if (list_length (ch->dst_list) == 0 && list_length (ch->src_list) != 0) {
	int vtx = ag->addVertex ();
	ag->mkOutput (vtx);
	list_iappend (ch->dst_list, vtx);
	list_iappend (ch->dst_list, -1);
      }
      for (listitem_t *li = list_first (ch->src_list); li;
	   li = list_next (li)) {
	int src_port = list_ivalue (list_next (li));
	for (listitem_t *mi = list_first (ch->dst_list); mi;
	     mi = list_next (mi)) {
	  ag->addEdge (list_ivalue (li), list_ivalue (mi), new EdgeInfo (buf, src_port, list_ivalue (list_next (mi))));
	  mi = list_next (mi);
	}
	li = list_next (li);
      }
      list_free (ch->src_list);
      list_free (ch->dst_list);
      FREE (ch);
    }
    phash_free (H);
    printDOT (ag, stdout, p->getName());
    delete ag;
  }
  return NULL;
}

void dflowgraph_free(ActPass *ap, void *v)
{
}

void dflowgraph_done(ActPass *ap)
{
  ActDynamicPass *dp = dynamic_cast<ActDynamicPass *>(ap);
}
