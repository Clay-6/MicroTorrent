#include <chrono>
#include <csignal>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/session_params.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/write_resume_data.hpp>
#include <thread>
#include "backend.hpp"

namespace {
    using clk = std::chrono::steady_clock;

    // set when we're exiting
    std::atomic<bool> shut_down{false};

    void sighandler(int) { shut_down = true; }

}  // anonymous namespace

int main(int argc, char const *argv[]) try {
    if (argc != 2) {
        std::cerr << "usage: " << argv[0] << " <added-url>" << std::endl;
        return 1;
    }

    // create the storage directory if it doesn't exist already
    if (!std::filesystem::exists(mt::storage_dir())) {
        std::filesystem::create_directory(mt::storage_dir());
    }

    // load session parameters
    std::vector<char> session_params = mt::load_file((mt::storage_dir() + "/.session").c_str());
    lt::session_params params = session_params.empty()
                                ? lt::session_params()
                                : lt::read_session_params(session_params);
    // receive error, storage, & status alerts
    params.settings.set_int(lt::settings_pack::alert_mask,
                            lt::alert_category::error |
                            lt::alert_category::storage |
                            lt::alert_category::status);

    lt::session ses(params);
    clk::time_point last_save_resume = clk::now();

    // load resume data from disk and pass it in as we add the added link
    std::vector<lt::add_torrent_params> resumes = mt::resume_torrents();

    lt::add_torrent_params added = mt::load_torrent(argv[1]);

    if (!resumes.empty()) {
        for (auto atp: resumes) {
            if (atp.info_hashes == added.info_hashes) {
                added = std::move(atp);
            }
        }
    }
    added.save_path = ".";  // save in current dir

    // add the passed added link and all the resumable torrents
    ses.async_add_torrent(std::move(added));
    for (auto atp: resumes) {
        if (!std::filesystem::exists(atp.save_path)) {
            // if the save path no longer exists, we need to start from scratch
            atp.total_downloaded = 0;
        }
        ses.async_add_torrent(std::move(atp));
    }

    // We'll add the handles here once they've been added
    std::vector<lt::torrent_handle> handles;

    std::signal(SIGINT, &sighandler);

    // set when we're exiting
    bool done = false;
    for (;;) {
        std::vector<lt::alert *> alerts;
        ses.pop_alerts(&alerts);

        if (shut_down) {
            shut_down = false;
            for (auto const &h: handles) {
                h.save_resume_data(
                        lt::torrent_handle::only_if_modified |
                        lt::torrent_handle::save_info_dict);
                done = true;
            }
        }

        for (lt::alert const *a: alerts) {
            if (auto at = lt::alert_cast<lt::add_torrent_alert>(a)) {
                handles.push_back(at->handle);
            }

            // if a torrent finishes, save its resume data
            if (auto alert = lt::alert_cast<lt::torrent_finished_alert>(a)) {
                alert->handle.save_resume_data(lt::torrent_handle::only_if_modified |
                                               lt::torrent_handle::save_info_dict);
            }

            // if we receive an error, give up
            if (auto alert = lt::alert_cast<lt::torrent_error_alert>(a)) {
                lt::torrent_handle h = alert->handle;
                std::cout << a->message() << std::endl;
                done = true;
                h.save_resume_data(lt::torrent_handle::only_if_modified |
                                   lt::torrent_handle::save_info_dict);
            }

            // when resume data is ready, save it
            if (auto rd = lt::alert_cast<lt::save_resume_data_alert>(a)) {
                mt::save_torrent_data(rd);
                if (done) goto done;
            }

            if (lt::alert_cast<lt::save_resume_data_failed_alert>(a)) {
                if (done) goto done;
            }

            if (auto st = lt::alert_cast<lt::state_update_alert>(a)) {
                if (st->status.empty()) continue;

                // we only have a single torrent, so we know which one
                // the status is for
                for (auto const &s: st->status) {
                    std::cout << '\r' << s.name << ": " << mt::state(s.state) << ' '
                              << (s.download_payload_rate / 1000) << " kB/s "
                              << (s.total_done / 1000) << " kB ("
                              << (s.progress_ppm / 10000) << "%) downloaded ("
                              << s.num_peers << " peers)\x1b[K";
                    std::cout.flush();
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        // ask the session to post a state_update_alert, to update our
        // state output for the torrent
        ses.post_torrent_updates();

        // save resume data once every 5 seconds
        if (clk::now() - last_save_resume > std::chrono::seconds(5)) {
            for (auto const &h: handles) {
                h.save_resume_data(lt::torrent_handle::only_if_modified |
                                   lt::torrent_handle::save_info_dict);
            }
            last_save_resume = clk::now();
        }
    }

    done:
    std::cout << "\nsaving session state" << std::endl;
    {
        std::ofstream of(mt::storage_dir() + "/.session", std::ios_base::binary);
        of.unsetf(std::ios_base::skipws);
        const std::vector<char> b = write_session_params_buf(ses.session_state(),
                                                             lt::save_state_flags_t::all());
        of.write(b.data(), int(b.size()));
    }

    std::cout << "\ndone, shutting down" << std::endl;
} catch (std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
}
