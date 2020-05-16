#include<linux/rbtree.h>

/*
*	Structure of each node in the rb_tree. It will hold a rb_object_t object which is used to store the key and data
*/
struct tree_node {
    struct rb_node node;
    rb_object_t rb_object; 
};

/*
*	Insert node function to add a node to the rbtree. 
*/
int insert_node(struct rb_root *root, struct tree_node *node)
{
  	struct rb_node **new = &(root->rb_node), *parent = NULL;

  	/* Figure out where to put new node */
  	while (*new) {
  		struct tree_node *this = container_of(*new, struct tree_node, node);
		parent = *new;

  		if (node->rb_object.key < this->rb_object.key)
  			new = &((*new)->rb_left);
  		else if (node->rb_object.key > this->rb_object.key)
  			new = &((*new)->rb_right);
  		else{
			  this->rb_object.data = node->rb_object.data; /*If key is already present, replace it will the new data*/
			  return true;
		  }
  	}

  	/* Add new node and rebalance tree. */
  	rb_link_node(&node->node, parent, new);
  	rb_insert_color(&node->node, root);

	return true;
}

/*
* Search function of rb_tree to find a node of given key.
* If key is present, it will return an appropriate node. If not, it will return NULL.
*/
struct rb_node *search_tree(struct rb_root *root, int key)
{
	struct rb_node *node = root->rb_node;

	while (node) {
		struct tree_node *data = container_of(node, struct tree_node, node);
		int result;

		result = key > data->rb_object.key;

		if (result < 0)
			node = node->rb_left;
		else if (result > 0)
			node = node->rb_right;
		else
			return node;
	}
	return NULL;
}