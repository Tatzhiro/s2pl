#include <iostream>

#define tNum 2

class Record
{
public:
    int value;
    int xlock;
    int slock;
    int lock;

    Record() : value(-1)
    {
    }
    Record() : xlock(0)
    {
    }
    Record() : slock(0)
    {
    }
    Record() : lock(0)
    {
    }
};

class Database
{
public:
    Record record[10];
};
Database *db;

enum Operation
{
    READ,
    WRITE
};

class Task
{
public:
    Operation op;
    int dataItem;

    Task(Operation ope, int idx)
    {
        op = ope;
        dataItem = idx;
    }
};

int CompareAndSwap(int *ptr, int expected, int update)
{
    int original = *ptr;
    if (original == expected)
        *ptr = update;
    return original;
}

int sCompareAndSwap(int *ptr, int expected)
{
    int original = *ptr;
    if (original <= expected)
        *ptr--;
    return original;
}

void cas_lock(int *lock)
{
    while (CompareAndSwap(lock, 0, 1) != 0)
        ;
}

void cas_slock(int *lock)
{
    while (sCompareAndSwap(lock, 0) == 1)
        ;
}

void updateLock(int *lock)
{
    while (CompareAndSwap(lock, -1, 1) != -1)
        ;
}

void unlock(Record *record)
{
    record->lock = 0;
}

class Transaction
{
public:
    void read(int dataItem)
    {
        // lock
        cas_slock(&db->record[dataItem].lock);
        //printf("lets read: %d\n", db->record[dataItem].value);
        std::cout << "lets read: " << db->record[dataItem].value << std::endl;
    }

    void write(int dataItem, int update)
    {
        if (update == 1)
        {
            updateLock(&db->record[dataItem].lock);
        }
        else
        {
            cas_lock(&db->record[dataItem].lock);
        }
        db->record[dataItem].value = 1;
    }
};

void *doTransaction(void *ops)
{
    Task **opts = (Task **)ops;
    Transaction t;
    int access[10];

    // growing phase
    for (int i = 0; i < 2; i++)
    {
        Task *op = opts[i];

        if (op->op == Operation::READ)
        {
            t.read(op->dataItem);
            access[op->dataItem] = 1;
        }
        else if (op->op == Operation::WRITE)
        {
            t.write(op->dataItem, access[op->dataItem]);
        }
    }
    // unlock
    for (int i = 0; i < 2; i++)
    {
        unlock(&db->record[i]);
    }
    return NULL;
}

int main()
{
    db = new Database();
    Task *opt[2][2] = {
        {new Task(READ, 0),
         new Task(WRITE, 1)},

        {new Task(READ, 1),
         new Task(WRITE, 0)},
    };

    pthread_t p[tNum];

    for (int i = 0; i < tNum; i++)
    {
        pthread_create(&p[i], NULL, doTransaction, opt[i]);
    }

    for (int i = 0; i < tNum; i++)
    {
        pthread_join(p[i], NULL);
    }
    for (int i = 0; i < 10; i++)
    {
        std::cout << i << ":" << db->record[i].value << std::endl;
        // printf("[%d]: %d\n", i, db->record[i].value);
    }
}