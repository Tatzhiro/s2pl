#include <iostream>

#define tNum 2
#define RECORD_SIZE 20
#define OPERATION_SET 10

class Lock
{
public:
    int flag;
    Lock() : flag(0)
    {
    }
    // exclusive lock
    // flag is 1 when lock-X is held
    void exclusiveLock()
    {
        int expected = 0;
        while (!__atomic_compare_exchange_n(&flag, &expected, 1, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
            ;
    }

    // shared lock
    // flag is -(number of readers) when shared lock is held
    void sharedLock()
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
        int expected = -1;
        while (!__atomic_compare_exchange_n(&flag, &expected, 1, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
            ;
    }

    void unlock(bool read)
    {
        if (read)
            __atomic_add_fetch(&flag, 1, __ATOMIC_SEQ_CST);
        else
            __atomic_store_n(&flag, 0, __ATOMIC_SEQ_CST);
    }
};

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

class Transaction
{
public:
    void read(int dataItem)
    {
        // lock
        db->record[dataItem].lock.sharedLock();
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
            db->record[dataItem].lock.exclusiveLock();
        }
        db->record[dataItem].value = 1;
    }
};

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
    for (int i = 0; i < RECORD_SIZE; i++)
    {
        db->record[i].lock.unlock(access[i]);
    }
    return NULL;
}

int main()
{
    db = new Database();
    Task *opt[tNum][OPERATION_SET] = {
        {
            // same operation cannot be generated
            // write read to same data cannot happen
            new Task(READ, 0),
            new Task(WRITE, 0),
            new Task(READ, 1),
            new Task(WRITE, 1),
            new Task(READ, 2),
            new Task(WRITE, 2),
            new Task(READ, 3),
            new Task(WRITE, 3),
            new Task(READ, 4),
            new Task(WRITE, 4),
        },

        {
            new Task(READ, 5),
            new Task(WRITE, 5),
            new Task(READ, 6),
            new Task(WRITE, 6),
            new Task(READ, 7),
            new Task(WRITE, 7),
            new Task(READ, 8),
            new Task(WRITE, 8),
            new Task(READ, 9),
            new Task(WRITE, 9),
        },
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