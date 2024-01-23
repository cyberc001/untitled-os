#include "thread_tree.h"
#include "cpu/x86/apic.h"

static thread_tree_node* thread_tree_rotate(thread_tree* tree, thread_tree_node* p, int dir)
{
	if(p == tree->root)
		p->parent = NULL;

	thread_tree_node* g = p->parent;
	thread_tree_node* s = p->child[1 - dir];
	if(!s)
		uart_printf("RBTREE rotation ERROR %p %p %d %u\r\n", tree, p, dir, lapic_read(LAPIC_REG_ID) >> 24);
	thread_tree_node* c = s->child[dir];

	p->child[1 - dir] = c; if(c) c->parent = p;
	s->child[dir] = p; p->parent = s;
	s->parent = g;
	if(g)
		g->child[p == g->child[TREE_DIR_RIGHT] ? TREE_DIR_RIGHT : TREE_DIR_LEFT] = s;
	else
		tree->root = s;
	return s;
}

static void thread_tree_insertp(thread_tree* tree, thread_tree_node* n, thread_tree_node* p, int dir)
{
	thread_tree_node *g,		// grandparent
			 *u;		// uncle

	tree->root->parent = NULL;

	n->clr = TREE_CLR_RED;
	n->child[TREE_DIR_LEFT] = n->child[TREE_DIR_RIGHT] = NULL;
	n->parent = p;
	if(!p) // inserting at root
	{ n->clr = TREE_CLR_BLACK; tree->root = n; return; }

	p->child[dir] = n;

	do{
		// case 1: parent is black, nothing to be done
		if(p->clr == TREE_CLR_BLACK)
			return;
		// parent is certainly red:

		if(!(g = p->parent))
		{ // case 4: parent is red and root
			p->clr = TREE_CLR_RED;
			return;
		}

		dir = TREE_DIR_CHILD(p); // at what side p is located in g
		u = g->child[1 - dir];
		if(!u || u->clr == TREE_CLR_BLACK)
		{ // case 5-6: parent is red and uncle is black
			if(n == p->child[1 - dir])
			{ // case 5: parent is red, uncle is black, n is inner grandchild
				thread_tree_rotate(tree, p, dir);
				n = p;
				p = g->child[dir];
			} // case 5 --> case 6
			// case 6: parent is red, uncle is black, n is outer grandchild	
			thread_tree_rotate(tree, g, 1 - dir);
			p->clr = TREE_CLR_BLACK;
			g->clr = TREE_CLR_RED;
			return;
		}

		// case 2: parent and uncle are red
		p->clr = TREE_CLR_BLACK;
		u->clr = TREE_CLR_BLACK;
		g->clr = TREE_CLR_RED;
		n = g;
	} while((p = n->parent));
}

void thread_tree_insert(thread_tree* tree, thread_tree_node* n)
{
	thread_tree_node* root = tree->root;
	if(!root){
		n->clr = TREE_CLR_BLACK;
		n->child[TREE_DIR_LEFT] = n->child[TREE_DIR_RIGHT] = NULL;
		n->parent = NULL;
		tree->root = n;
		return;
	}

	while(root)
	{
		int dir = n->thr->vruntime < root->thr->vruntime ? TREE_DIR_LEFT : TREE_DIR_RIGHT;
		if(!root->child[dir]){
			root->child[dir] = n;
			n->child[TREE_DIR_LEFT] = n->child[TREE_DIR_RIGHT] = NULL;
			thread_tree_insertp(tree, n, root, dir);
			return;
		}
		root = root->child[dir];
	}
}

thread_tree_node* rbdelete2(thread_tree* tree, thread_tree_node* n);
thread_tree_node* thread_tree_delete(thread_tree* tree, thread_tree_node* n)
{
	tree->root->parent = NULL;

	if(n->child[TREE_DIR_LEFT] && n->child[TREE_DIR_RIGHT]){ // 2 children
		thread_tree_node* s = n->child[TREE_DIR_RIGHT]; //successor
		while(s->child[TREE_DIR_LEFT])
			s = s->child[TREE_DIR_LEFT];
		n->thr = s->thr;
		n->thr->hndl = n;
		return thread_tree_delete(tree, s);
	}
	else if(n->child[TREE_DIR_LEFT] || n->child[TREE_DIR_RIGHT]){ // 1 child
		if(n->parent){
			int dir = TREE_DIR_CHILD(n);
			thread_tree_node* child_replace = n->child[n->child[TREE_DIR_LEFT] ? TREE_DIR_LEFT : TREE_DIR_RIGHT];
			n->parent->child[dir] = child_replace;
			child_replace->clr = TREE_CLR_BLACK;
			child_replace->parent = n->parent;
		}
		else {
			tree->root = n->child[n->child[TREE_DIR_LEFT] ? TREE_DIR_LEFT : TREE_DIR_RIGHT];
		}
		return n;
	}
	else { // no children
		int dir = TREE_DIR_CHILD(n);
		if(n == tree->root){
			tree->root = NULL;
			return n;
		}
		else if(n->clr == TREE_CLR_RED){
			n->parent->child[dir] = NULL;
			return n;
		}
		else
			return rbdelete2(tree, n);
	}
}

thread_tree_node* rbdelete2(thread_tree* tree, thread_tree_node* n)
{
	thread_tree_node* p = n->parent;
	thread_tree_node *s, *c, *d;
	//uart_printf("START %p %p\r\n", n, p);
	//thread_tree_print(tree);
	int dir = TREE_DIR_CHILD(n);
	p->child[dir] = NULL;
	goto start_d;

	do {
		dir = TREE_DIR_CHILD(n);
start_d:
		s = p->child[1-dir];
		/*if(!s){ 
			uart_printf("BAD %p %p %p\r\n", n, p, p->child[dir-1]);
			thread_tree_print(tree);
		}*/
		d = s->child[1-dir];
		c = s->child[dir];
		if(s->clr == TREE_CLR_RED)
			goto case_d3;
		if(d && d->clr == TREE_CLR_RED)
			goto case_d6;
		if(c && c->clr == TREE_CLR_RED)
			goto case_d5;
		if(p->clr == TREE_CLR_RED)
			goto case_d4;
		goto case_d2;
case_d3:
		thread_tree_rotate(tree, p, dir);
		p->clr = TREE_CLR_RED;
		s->clr = TREE_CLR_BLACK;
		s = c;
		d = s->child[1 - dir];
		if(d && d->clr == TREE_CLR_RED)
			goto case_d6;
		c = s->child[dir];
		if(c && c->clr == TREE_CLR_RED)
			goto case_d5;
case_d4:
		s->clr = TREE_CLR_RED;
		p->clr = TREE_CLR_BLACK;
		return n;
case_d5:
		thread_tree_rotate(tree, s, 1 - dir);
		s->clr = TREE_CLR_RED;
		c->clr = TREE_CLR_BLACK;
		d = s;
		s = c;
case_d6:
		thread_tree_rotate(tree, p, dir);
		s->clr = p->clr;
		p->clr = TREE_CLR_BLACK;
		d->clr = TREE_CLR_BLACK;
		return n;

case_d2:
		s->clr = TREE_CLR_RED;
		n = p;
	} while((p = n->parent));

	// case_d1
	return n;
}

void thread_tree_print_r(thread_tree_node* n, unsigned depth)
{
	for(unsigned i = 0; i < depth; ++i) uart_putchar('\t');
	if(!n){
		uart_printf("--\r\n"); // it's a leaf
		return;
	}
	uart_printf("%c w %lu vr %lu addr %p (thr %p) (par %p)\r\n", n->clr == TREE_CLR_BLACK ? 'B' : 'R', n->thr->weight, n->thr->vruntime, n, n->thr, n->parent);

	thread_tree_print_r(n->child[TREE_DIR_LEFT], depth + 1);
	thread_tree_print_r(n->child[TREE_DIR_RIGHT], depth + 1);
}
