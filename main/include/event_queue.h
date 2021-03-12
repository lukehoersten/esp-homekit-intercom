static const char *TAG = "HAP Intercom";

void intercom_event_queue_bell_ring();

void intercom_event_queue_lock_unsecure();

void intercom_event_queue_lock_secure();

void intercom_event_queue_lock_timeout();

void intercom_event_queue_run();

bool intercom_event_queue_init();
