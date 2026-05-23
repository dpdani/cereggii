::: cereggii._cereggii.AtomicPartitionedQueue
    options:
        members:
            - __init__
            - put
            - get
            - try_get
            - put_many
            - get_many
            - try_get_many
            - close
            - closed
            - approx_len
            - producer
            - consumer

::: cereggii._cereggii.AtomicPartitionedQueueProducer
    options:
        members:
            - put
            - put_many

::: cereggii._cereggii.AtomicPartitionedQueueConsumer
    options:
        members:
            - get
            - try_get
            - get_many
            - try_get_many
