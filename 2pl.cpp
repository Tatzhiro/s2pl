#include <iostream>

#define tNum 2
#define RECORD_SIZE 10
#define OPERATION_SET 2

class Record
{
public:
    int value;
    Lock lock;

    Record() : value(-1)
    {
    }
};

class Database
{
public:
    Record record[RECORD_SIZE];
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

class Lock
{
public:
    int flag;
    Lock() : flag(0)
    {
    }
    /*
    flag == 1, write locked;
    flag == 0, not locked;
    flag < 0, there are |$flag| readers who acquires read-lock.
    */
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

    // exclusive lock
    void cas_lock()
    {
        int expected = 0;
        while (!__atomic_compare_exchange_n(&flag, &expected, 1, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
            ;
    }

    // shared lock 困った
    void cas_slock()
    {
        while (true)
        {
            int my_flag = __atomic_load_n(&flag, __ATOMIC_SEQ_CST);

            if (my_flag <= 0)
            {
                if (__atomic_compare_exchange_n(
                        &flag,
                        &my_flag,
                        my_flag - 1,
                        false,
                        __ATOMIC_SEQ_CST,
                        __ATOMIC_SEQ_CST))
                {
                    break;
                };
            }
        }
    }

    void updateLock()
    {
        while (CompareAndSwap(&flag, -1, 1) != -1)
            ;
    }

    void unlock()
    {
        if (flag < 0)
            flag++;
        else
            flag = 0;
    }
};

class Transaction
{
public:
    void read(int dataItem)
    {
        // lock
        db->record[dataItem].lock.cas_slock();
        //printf("lets read: %d\n", db->record[dataItem].value);
        std::cout << "lets read: " << db->record[dataItem].value << std::endl;
    }

    void write(int dataItem, int update)
    {
        if (update == 1)
        {
            db->record[dataItem].lock.updateLock();
        }
        else
        {
            db->record[dataItem].lock.cas_lock();
        }
        db->record[dataItem].value = 1;
    }
};

void *doTransaction(void *ops)
{
    Task **opts = (Task **)ops;
    Transaction t;
    // used for updates
    int access[RECORD_SIZE];

    // growing phase
    for (int i = 0; i < OPERATION_SET; i++)
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
    for (int i = 0; i < OPERATION_SET; i++)
    {
        db->record[i].lock.unlock();
    }
    return NULL;
}

int main()
{
    db = new Database();
    Task *opt[tNum][OPERATION_SET] = {
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
    for (int i = 0; i < RECORD_SIZE; i++)
    {
        std::cout << i << ":" << db->record[i].value << std::endl;
        // printf("[%d]: %d\n", i, db->record[i].value);
    }
}