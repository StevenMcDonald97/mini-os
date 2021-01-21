#include "lib.h"
#include "stddef.h"
#include "debug.h"

// manage a linked list 

// add an item to the end of the list
void append_list_tail(struct HeadList *list, struct List *item){
	item->next = NULL;
	// if empty head and tail are both this item 
	if (is_list_empty(list)){
		list->next = item;
		list->tail = item;
	} else {
		list->tail->next = item;
		list->tail = item;
	}
}

// remove item from beginning of list 
struct List* remove_list_head(struct HeadList *list){
	struct List *item;

	if (is_list_empty(list)){
		return NULL;
	}

	item = list->next;
	list->next = item->next;

	if (list->next == NULL){
		list->tail = NULL;
	}

	return item;


}


bool is_list_empty(struct HeadList *list){
	return (list->next == NULL);
}