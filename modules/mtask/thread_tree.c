#include "thread_tree.h"

static thread_tree_node* thread_tree_rotate(thread_tree* tree, thread_tree_node* p, int dir)
{
	thread_tree_node* g = p->parent;
	thread_tree_node* s = p->child[1 - dir];
	thread_tree_node* c = s ? s->child[dir] : NULL;

	p->child[1 - dir] = c;
	if(c) c->parent = p;
	if(s) s->child[dir] = p;
	p->parent = s;
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

static thread_tree_node* thread_tree_delete_replacement(thread_tree_node* n)
{
	if(n->child[TREE_DIR_LEFT] && n->child[TREE_DIR_RIGHT]){
		while(n->child[TREE_DIR_LEFT])
			n = n->child[TREE_DIR_LEFT];
		return n;
	}
	else if(!n->child[TREE_DIR_LEFT] && !n->child[TREE_DIR_RIGHT])
		return NULL;
	else
		return n->child[TREE_DIR_LEFT] ? n->child[TREE_DIR_LEFT] : n->child[TREE_DIR_RIGHT];
}
static void thread_tree_delete_fixbb(thread_tree* tree, thread_tree_node* n) // fix 2 black nodes in a row
{
	if(n == tree->root)
		return;

	thread_tree_node *s = TREE_GET_SIBLING(n), *p = n->parent;
	if(!s){
		// no sibling - proceed up the tree
		thread_tree_delete_fixbb(tree, p);
	}
	else{
		if(s->clr == TREE_CLR_RED){
			p->clr = TREE_CLR_RED;
			s->clr = TREE_CLR_BLACK;
			thread_tree_rotate(tree, p, TREE_DIR_CHILD(s));
			thread_tree_delete_fixbb(tree, n);
		}
		else{
			if((s->child[TREE_DIR_LEFT] && s->child[TREE_DIR_LEFT]->clr  == TREE_CLR_RED)
			|| (s->child[TREE_DIR_RIGHT] && s->child[TREE_DIR_RIGHT]->clr == TREE_CLR_RED))
			{ // at least 1 red child
				if(s->child[TREE_DIR_LEFT] && s->child[TREE_DIR_LEFT]->clr == TREE_CLR_RED)
				{ // on the left
					if(TREE_DIR_CHILD(s) == TREE_DIR_LEFT)
					{ // left left
					  	s->child[TREE_DIR_LEFT]->clr = s->clr;
						s->clr = p->clr;
						thread_tree_rotate(tree, p, TREE_DIR_RIGHT);
					}
					else
					{ // right left
						s->child[TREE_DIR_LEFT]->clr = p->clr;
						thread_tree_rotate(tree, s, TREE_DIR_RIGHT);
						thread_tree_rotate(tree, p, TREE_DIR_LEFT);
					}
				}
				else
				{ // on the right
					if(TREE_DIR_CHILD(s) == TREE_DIR_LEFT)
					{ // left right
						s->child[TREE_DIR_RIGHT]->clr = p->clr;
						thread_tree_rotate(tree, s, TREE_DIR_LEFT);
						thread_tree_rotate(tree, p, TREE_DIR_RIGHT);
					}
					else
					{ // right right
						s->child[TREE_DIR_RIGHT]->clr = s->clr;
						s->clr = p->clr;
						thread_tree_rotate(tree, p, TREE_DIR_LEFT);
					}
				}
				p->clr = TREE_CLR_BLACK;
			}
			else
			{ // 2 black children
				s->clr = TREE_CLR_RED;
				if(p->clr == TREE_CLR_BLACK)
					thread_tree_delete_fixbb(tree, n);
				else
					p->clr = TREE_CLR_BLACK;
			}
		}
	}
}

thread_tree_node* thread_tree_delete(thread_tree* tree, thread_tree_node* n)
{
	while(n)
	{
		thread_tree_node* u = thread_tree_delete_replacement(n);
		int un_is_black = (!u || u->clr == TREE_CLR_BLACK) && n->clr == TREE_CLR_BLACK;

		thread_tree_node* p = n->parent;

		if(!u){
			if(n == tree->root)
				tree->root = NULL;
			else{
				if(un_is_black)
					thread_tree_delete_fixbb(tree, n);
				else
					if(TREE_GET_SIBLING(n))
						TREE_GET_SIBLING(n)->clr = TREE_CLR_RED;

				// delete n
				p->child[TREE_DIR_CHILD(n)] = NULL;
			}
			return n;
		}
		
		if(!n->child[TREE_DIR_LEFT] || !n->child[TREE_DIR_RIGHT]){
			// n has only 1 child
			if(n == tree->root)
			{ // replace n with it's child if n == root	
				n->thr = u->thr;
				n->thr->hndl = n;
				n->child[TREE_DIR_LEFT] = n->child[TREE_DIR_RIGHT] = NULL;
				return u;
			}
			else
			{ // delete n from the tree and move u up
				p->child[TREE_DIR_CHILD(n)] = u;
				u->parent = p;
				if(un_is_black)
					thread_tree_delete_fixbb(tree, n);
				else
					u->clr = TREE_CLR_BLACK;
				return n;
			}
		}
		// swap u and n
		thread_tree_node tmp;
		tmp.thr = n->thr;
		n->thr = u->thr;
		u->thr = tmp.thr;
		n->thr->hndl = n;
		u->thr->hndl = u;
		n = u;
	}
	return NULL;
}

void thread_tree_print_r(thread_tree_node* n, unsigned depth)
{
	for(unsigned i = 0; i < depth; ++i) uart_putchar('\t');
	if(!n){
		uart_printf("--\r\n"); // it's a leaf
		return;
	}
	uart_printf("%c w %lu vr %lu addr %p (thr %p)\r\n", n->clr == TREE_CLR_BLACK ? 'B' : 'R', n->thr->weight, n->thr->vruntime, n, n->thr);

	thread_tree_print_r(n->child[TREE_DIR_LEFT], depth + 1);
	thread_tree_print_r(n->child[TREE_DIR_RIGHT], depth + 1);
}
