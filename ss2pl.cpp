#include <iostream>
#include <iterator> // for iterators
#include <vector>   // for vectors
#include <algorithm>

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
    bool exclusiveLock()
    {
        int expected = 0;
        return __atomic_compare_exchange_n(&flag, &expected, 1, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    }

    // shared lock
    // flag is -(number of readers) when shared lock is held
    bool sharedLock()
    {
        while (true)
        {
            int my_flag = __atomic_load_n(&flag, __ATOMIC_SEQ_CST);
            if (my_flag <= 0)
            {
                if (__atomic_compare_exchange_n(&flag, &my_flag, my_flag - 1, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
                {
                    return true;
                };
            }
            else
                return false;
        }
    }

    bool updateLock()
    {
        int expected = -1;
        return __atomic_compare_exchange_n(&flag, &expected, 1, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    }

    void exclusiveUnlock()
    {
        __atomic_store_n(&flag, 0, __ATOMIC_SEQ_CST);
    }

    void sharedUnlock()
    {
        __atomic_add_fetch(&flag, 1, __ATOMIC_SEQ_CST);
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

enum TxStatus
{
    COMMITTED,
    ABORTED,
    OPERATING
};

class ReadSet
{
public:
    Record *record;

    ReadSet(Record *rec) : record(rec) {}
};

class WriteSet
{
public:
    Record *record;
    int index;
    WriteSet(Record *rec) : record(rec) {}
};

class Transaction
{
public:
    TxStatus status;
    std::vector<ReadSet> read_set;
    std::vector<WriteSet> write_set;

    void read(int dataItem)
    {
        // lock
        if (db->record[dataItem].lock.sharedLock())
        {
            read_set.emplace_back(&db->record[dataItem]);
        }
        else
        {
            status = TxStatus::ABORTED;
        }

        // std::cout << "lets read: " << db->record[dataItem].value << std::endl;
    }

    bool check(int dataItem)
    {
        for (auto &read : read_set)
        {
            if (read.record == &db->record[dataItem])
            {
                return true;
            }
        }
        return false;
    }

    void write(int dataItem)
    {
        if (check(dataItem))
        {
            if (db->record[dataItem].lock.updateLock())
            {

                db->record[dataItem].value = 1;
                int i = 0;
                for (auto &read : read_set)
                {
                    if (read.record == &db->record[dataItem])
                    {
                        read_set.erase(read_set.begin() + i);
                        write_set.emplace_back(&db->record[dataItem]);
                        break;
                    }
                    i++;
                }
            }
            else
            {
                status = TxStatus::ABORTED;
            }
        }
        else
        {
            if (db->record[dataItem].lock.exclusiveLock())
            {
                db->record[dataItem].value = 1;
                write_set.emplace_back(&db->record[dataItem]);
            }
            else
            {
                status = TxStatus::ABORTED;
            }
        }
    }

    void begin()
    {
        status = TxStatus::OPERATING;
    }

    void aborted()
    {
        shrinkingPhase();
    }

    void commit()
    {
        shrinkingPhase();
        status = TxStatus::COMMITTED;
    }

private:
    void shrinkingPhase()
    {
        for (auto &read : read_set)
        {
            read.record->lock.sharedUnlock();
        }
        for (auto &write : write_set)
        {
            write.record->lock.exclusiveUnlock();
        }
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

RETRY:
    Transaction t;
    // used for updates
    t.begin();

    // growing phase
    for (int i = 0; i < OPERATION_SET; i++)
    {
        Task *op = opts[i];

        if (op->op == Operation::READ)
        {
            t.read(op->dataItem);
        }
        else if (op->op == Operation::WRITE)
        {
            t.write(op->dataItem);
        }

        if (t.status == TxStatus::ABORTED)
        {
            t.aborted();
            goto RETRY;
        }
    }

    t.commit();
    return NULL;
}

void produceOp()
{
    for (int i = 0; i < tNum; i++)
    {
        for (int j = 0; j < OPERATION_SET; j++)
        {
            if (rand() <= 1)
                new Task(READ, rand());
        }
    }
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