#include "channel_bench.h"
#include "hdr_histogram.h"

DOCA_LOG_REGISTER(SYNC_EVENT);

void
handle_sync_sender(channel_config *config, DOCA::bench_runner *runner, DOCA::bench_stat *stat, DOCA::dev *dev,
                   DOCA::comm_channel_ep *ep,
                   DOCA::comm_channel_addr *peer_addr) {
    _unused(runner);
    _unused(stat);
    auto *publish_config = new sync_event_config();
    publish_config->se = new DOCA::sync_event(false, dev);
    publish_config->se_ctx = new DOCA::ctx(publish_config->se, {});
    publish_config->se_workq = new DOCA::workq(config->batch_size, publish_config->se);


    auto *sub_config = new sync_event_config();
    sub_config->se = new DOCA::sync_event(true, dev);
    sub_config->se_ctx = new DOCA::ctx(sub_config->se, {});
    sub_config->se_workq = new DOCA::workq(config->batch_size, sub_config->se);

    handle_sync_event_export(publish_config->se, ep, peer_addr);
    handle_sync_event_export(sub_config->se, ep, peer_addr);

    size_t se_add_pub_job_fetched = 0;
    DOCA::sync_event_job_update_add se_add_job(publish_config->se);
    se_add_job.set_value_and_fetched(1, &se_add_pub_job_fetched);


    DOCA::sync_event_job_update_set se_set_pub_job(publish_config->se);
    se_set_pub_job.set_value(0);

    size_t se_get_pub_job_fetched = 0, se_get_sub_job_fetched = 0;
    DOCA::sync_event_job_get se_get_pub_job(publish_config->se);
    DOCA::sync_event_job_get se_get_sub_job(sub_config->se);
    se_get_pub_job.set_fetched(&se_get_pub_job_fetched);
    se_get_sub_job.set_fetched(&se_get_sub_job_fetched);


    size_t wait_index = 0;

    auto *hist_get_pub = new DOCA::Histogram(10, 10000);
    auto *hist_get_sub = new DOCA::Histogram(10, 10000);
    auto *hist_set_sync = new DOCA::Histogram(10, 10000);
    auto *hist_set_async = new DOCA::Histogram(10, 10000);
    auto *hist_add_sync = new DOCA::Histogram(10, 10000);
    auto *hist_add_async = new DOCA::Histogram(10, 10000);

    DOCA::Timer timer;

    ::doca_event event{};
    doca_error_t result;
    try {
        while (true) {
            timer.tic();
            publish_config->sync_event_job_submit_sync(&se_get_pub_job);
            hist_get_pub->record(timer.toc_ns());

            timer.tic();
            sub_config->sync_event_job_submit_sync(&se_get_sub_job);
            hist_get_sub->record(timer.toc_ns());

            // std::cout << se_get_pub_job_fetched << " " << se_get_sub_job_fetched << std::endl;
            if (se_get_sub_job_fetched == wait_index) {
                if (wait_index == 100000) {
                    break;
                }

                timer.tic();
                publish_config->sync_event_job_submit_async(&se_add_job);
                hist_add_async->record(timer.toc_ns());

                while ((result = publish_config->se_workq->poll_completion(&event)) == DOCA_ERROR_AGAIN) {
                }
                if (result != DOCA_SUCCESS) {
                    throw DOCA::DOCAException(result, __FILE__, __LINE__);
                }

                usleep(1);

                timer.tic();
                publish_config->sync_event_job_submit_sync(&se_add_job);
                hist_add_sync->record(timer.toc_ns());

                usleep(1);

                timer.tic();
                se_set_pub_job.set_value(wait_index + 3);
                publish_config->sync_event_job_submit_sync(&se_set_pub_job);
                hist_set_sync->record(timer.toc_ns());


                timer.tic();
                se_set_pub_job.set_value(wait_index + 4);
                publish_config->sync_event_job_submit_async(&se_set_pub_job);
                hist_set_async->record(timer.toc_ns());

                while ((result = publish_config->se_workq->poll_completion(&event)) == DOCA_ERROR_AGAIN) {
                }
                if (result != DOCA_SUCCESS) {
                    throw DOCA::DOCAException(result, __FILE__, __LINE__);
                }

                usleep(1);


                stat->finish_one_op();
                wait_index += 4;
            }
        }
    }
    catch (const std::exception &e) {
        DOCA_LOG_WARN("%s", e.what());
    }

    std::cout << "get_pub latency:" << std::endl;
    hist_get_pub->print(stdout, 5);

    std::cout << "get_sub latency:" << std::endl;
    hist_get_sub->print(stdout, 5);

    std::cout << "set_sync latency:" << std::endl;
    hist_set_sync->print(stdout, 5);

    std::cout << "set_async latency:" << std::endl;
    hist_set_async->print(stdout, 5);

    std::cout << "add_sync latency:" << std::endl;
    hist_add_sync->print(stdout, 5);

    std::cout << "add_async latency:" << std::endl;
    hist_add_async->print(stdout, 5);

    delete sub_config;
    delete publish_config;
    delete hist_get_pub;
    delete hist_get_sub;
    delete hist_set_sync;
    delete hist_set_async;
    delete hist_add_sync;
    delete hist_add_async;
}

void handle_sync_receiver(channel_config *config, DOCA::bench_runner *runner, DOCA::bench_stat *stat, DOCA::dev *dev,
                          DOCA::comm_channel_ep *ep,
                          DOCA::comm_channel_addr *peer_addr) {
    _unused(runner);
    _unused(stat);

    auto *sub_config = new sync_event_config();
    sub_config->se = handle_sync_event_import(dev, ep, peer_addr);
    sub_config->se_ctx = new DOCA::ctx(sub_config->se, {});
    sub_config->se_workq = new DOCA::workq(config->batch_size, sub_config->se);

    auto *publish_config = new sync_event_config();
    publish_config->se = handle_sync_event_import(dev, ep, peer_addr);
    publish_config->se_ctx = new DOCA::ctx(publish_config->se, {});
    publish_config->se_workq = new DOCA::workq(config->batch_size, publish_config->se);

    size_t se_add_pub_job_fetched = 0;
    DOCA::sync_event_job_update_add se_add_job(publish_config->se);
    se_add_job.set_value_and_fetched(1, &se_add_pub_job_fetched);


    DOCA::sync_event_job_update_set se_set_pub_job(publish_config->se);
    se_set_pub_job.set_value(0);

    size_t se_get_pub_job_fetched = 0, se_get_sub_job_fetched = 0;
    DOCA::sync_event_job_get se_get_pub_job(publish_config->se);
    DOCA::sync_event_job_get se_get_sub_job(sub_config->se);
    se_get_pub_job.set_fetched(&se_get_pub_job_fetched);
    se_get_sub_job.set_fetched(&se_get_sub_job_fetched);


    size_t wait_index = 0;

    auto *hist_get_pub = new DOCA::Histogram(10, 10000);
    auto *hist_get_sub = new DOCA::Histogram(10, 10000);
    auto *hist_set_sync = new DOCA::Histogram(10, 10000);
    auto *hist_set_async = new DOCA::Histogram(10, 10000);
    auto *hist_add_sync = new DOCA::Histogram(10, 10000);
    auto *hist_add_async = new DOCA::Histogram(10, 10000);

    DOCA::Timer timer;

    ::doca_event event{};
    doca_error_t result;

    try {
        while (true) {
            timer.tic();
            publish_config->sync_event_job_submit_sync(&se_get_pub_job);
            hist_get_pub->record(timer.toc_ns());

            timer.tic();
            sub_config->sync_event_job_submit_sync(&se_get_sub_job);
            hist_get_sub->record(timer.toc_ns());

            if (wait_index == 100000) {
                break;
            }
            // std::cout << se_get_pub_job_fetched << " " << se_get_sub_job_fetched << std::endl;

            if (se_get_sub_job_fetched == wait_index + 4) {

                timer.tic();
                publish_config->sync_event_job_submit_async(&se_add_job);
                hist_add_async->record(timer.toc_ns());

                while ((result = publish_config->se_workq->poll_completion(&event)) == DOCA_ERROR_AGAIN) {
                }
                if (result != DOCA_SUCCESS) {
                    throw DOCA::DOCAException(result, __FILE__, __LINE__);
                }

                usleep(1);

                timer.tic();
                publish_config->sync_event_job_submit_sync(&se_add_job);
                hist_add_sync->record(timer.toc_ns());

                usleep(1);

                timer.tic();
                se_set_pub_job.set_value(wait_index + 3);
                publish_config->sync_event_job_submit_sync(&se_set_pub_job);
                hist_set_sync->record(timer.toc_ns());

                usleep(1);


                timer.tic();
                se_set_pub_job.set_value(wait_index + 4);
                publish_config->sync_event_job_submit_async(&se_set_pub_job);
                hist_set_async->record(timer.toc_ns());
                while ((result = publish_config->se_workq->poll_completion(&event)) == DOCA_ERROR_AGAIN) {
                }
                if (result != DOCA_SUCCESS) {
                    throw DOCA::DOCAException(result, __FILE__, __LINE__);
                }


                stat->finish_one_op();
                wait_index += 4;

            }
        }
    }
    catch (const std::exception &e) {
        DOCA_LOG_WARN("%s", e.what());
    }


    std::cout << "get_pub latency:" << std::endl;
    hist_get_pub->print(stdout, 5);

    std::cout << "get_sub latency:" << std::endl;
    hist_get_sub->print(stdout, 5);

    std::cout << "set_sync latency:" << std::endl;
    hist_set_sync->print(stdout, 5);

    std::cout << "set_async latency:" << std::endl;
    hist_set_async->print(stdout, 5);

    std::cout << "add_sync latency:" << std::endl;
    hist_add_sync->print(stdout, 5);

    std::cout << "add_async latency:" << std::endl;
    hist_add_async->print(stdout, 5);

    delete sub_config;
    delete publish_config;
    delete hist_get_pub;
    delete hist_get_sub;
    delete hist_set_sync;
    delete hist_set_async;
    delete hist_add_sync;
    delete hist_add_async;
}