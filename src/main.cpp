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
#include <slint.h>
#include <msd/channel.hpp>


#include "backend.hpp"
#include "window.h"

namespace {
    using clk = std::chrono::steady_clock;

    // set when we're exiting
    std::atomic<bool> shut_down{false};

    void sighandler(int) { shut_down = true; }

}  // anonymous namespace

void event_loop(lt::session &ses, clk::time_point last_save_resume, slint::ComponentWeakHandle<MainWindow> ui_weak,
                msd::channel<mt::add_request> &add_reqs, msd::channel<mt::remove_request> &del_reqs) {
    // We'll add the handles here once they've been added
    std::vector<lt::torrent_handle> handles;
    auto infos = std::make_shared<slint::VectorModel<TorrentInfo>>(); // info for all the torrents to be displayed in the UI
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

        while (!add_reqs.empty()) {
            mt::add_request req{};
            add_reqs >> req;
            // if it has quotes around it, we WILL die so must remove them
            if (req.uri.starts_with('"') || req.uri.starts_with('\'')) {
                req.uri.erase(0, 1);
            }
            if (req.uri.ends_with('"') || req.uri.ends_with('\'')) {
                req.uri.erase(req.uri.size() - 1, 1);
            }
            // if this fails, we don't want to just crash
            try {
                lt::add_torrent_params atp = mt::load_torrent(req.uri);
                // save to the current directory if the save path is empty
                atp.save_path = req.save_path.empty() ? "." : req.save_path;
                ses.async_add_torrent(atp);
            } catch (lt::system_error &e) {
                slint::invoke_from_event_loop([req, &ui_weak]() {
                    std::string msg = "Torrent '" + req.uri + "' is invalid";
                    auto ui = *ui_weak.lock();
                    ui->set_error_message(slint::SharedString(msg));
                    ui->invoke_show_error();
                });
            }
        }

        while (!del_reqs.empty()) {
            mt::remove_request req{};
            del_reqs >> req;
            for (auto const &h: handles) {
                if (h.id() == req.id) {
                    ses.remove_torrent(h);
                    break;
                }
            }
        }

        // handle the alerts
        for (lt::alert const *a: alerts) {
            if (auto at = lt::alert_cast<lt::add_torrent_alert>(a)) {
                handles.push_back(at->handle);

                TorrentInfo new_info;
                new_info.name = at->params.name;
                new_info.ses_id = at->handle.id();
                // We can't get the downloaded bytes or progress at this stage, so initialise them to 0
                new_info.downloaded_bytes = 0;
                new_info.total_bytes = 0;

                slint::invoke_from_event_loop([new_info, &infos, &ui_weak]() {
                    infos->push_back(new_info);
                    auto ui = ui_weak.lock();
                    ui.value()->set_torrents(infos);
                });
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

                for (auto const &s: st->status) {
                    // print to console for debugging
                    std::cout << '\r' << s.name << ": " << mt::state(s.state) << ' '
                              << (s.download_payload_rate / 1000) << " kB/s "
                              << (s.total_done / 1000) << " kB ("
                              << (s.progress_ppm / 10000) << "%) downloaded ("
                              << s.num_peers << " peers)\x1b[K";
                    std::cout.flush();

                    auto id = s.handle.id();
                    for (int i = 0; i < infos->row_count(); i++) {
                        auto info = *infos->row_data(i);
                        if (info.ses_id == id) {
                            info.downloaded_bytes = s.total_done;
                            info.total_bytes = s.total_wanted;

                            slint::invoke_from_event_loop([i, info, infos, &ui_weak]() {
                                infos->set_row_data(i, info);
                                auto ui = *ui_weak.lock();
                                ui->set_torrents(infos);
                            });
                            break;
                        }
                    }

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
}

int main(int argc, char const *argv[]) try {
    auto ui = MainWindow::create();

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

    // add all the resumable torrents
    for (auto atp: resumes) {
        if (!std::filesystem::exists(atp.save_path)) {
            // if the save path no longer exists, we need to start from scratch
            atp.total_downloaded = 0;
        }
        ses.async_add_torrent(std::move(atp));
    }


    std::signal(SIGINT, &sighandler);

    // set up channels
    msd::channel<mt::add_request> add_channel;
    msd::channel<mt::remove_request> remove_channel;

    // set up request callbacks
    ui->on_add_torrent([&](const auto &torrent, const auto &save_path) {
        mt::add_request req{std::string(torrent), std::string(save_path)};
        add_channel << req;
    });

    slint::ComponentWeakHandle<MainWindow> ui_weak(ui);

    std::thread event_thread{[ui_weak, &ses, &last_save_resume, &add_channel, &remove_channel]() {
        event_loop(ses, last_save_resume, ui_weak, add_channel, remove_channel);
    }};


    ui->run();

    // upon returning, the window has been closed so we need to stop the event loop
    shut_down = true;
    event_thread.join();
} catch (std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
}
