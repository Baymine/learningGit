#include <iostream>

struct ListNode{
    int val;
    ListNode* next;
    ListNode(int val) : val(0), next(nullptr);
};


int main(int argc, char** argv) {
    ListNode* head = new ListNode(0);
    ListNode* node = head;

    int val = 0;
    while(cin >> val){
        node->next = new ListNode(val);
        node = node->next;
    }   

    node = head->next;
    while (node != nullptr){
        cout << node->val << " ";
        node = node->next;
    }

    return 0;
}
