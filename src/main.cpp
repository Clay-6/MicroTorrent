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
#include <libtorrent/ip_filter.hpp>


#include "backend.hpp"
#include "window.h"

namespace {
    using clk = std::chrono::steady_clock;

    // set when we're exiting
    std::atomic<bool> shut_down{false};

    void sighandler(int) { shut_down = true; }

}  // anonymous namespace

void event_loop(lt::session &ses, clk::time_point last_save_resume, slint::ComponentWeakHandle<MainWindow> ui_weak,
                msd::channel<mt::add_request> &add_reqs, msd::channel<mt::remove_request> &del_reqs,
                msd::channel<mt::create_request> &create_reqs,
                msd::channel<mt::update_blacklist_request> &blacklist_updates) {
    // info for all the torrents to be displayed in the UI
    auto infos = std::make_shared<slint::VectorModel<TorrentInfo>>();
    // set when we're exiting
    bool done = false;
    for (;;) {
        std::vector<lt::alert *> alerts;
        ses.pop_alerts(&alerts);

        if (shut_down) {
            shut_down = false;
            done = true;
            for (auto const &h: ses.get_torrents()) {
                h.save_resume_data(
                        lt::torrent_handle::only_if_modified |
                        lt::torrent_handle::save_info_dict);
            }
        }

        while (!add_reqs.empty()) {
            mt::add_request req{};
            add_reqs >> req;

            // if this fails, we don't want to just crash
            try {
                lt::add_torrent_params atp = mt::load_torrent(req.uri);
                // save to the current directory if the save path is empty
                atp.save_path = req.save_path.empty() ? "." : req.save_path;
                ses.async_add_torrent(atp);
            } catch (lt::system_error &e) {
                std::string msg = "Torrent '" + req.uri + "' is invalid";
                slint::invoke_from_event_loop([msg, &ui_weak]() {
                    auto ui = *ui_weak.lock();
                    ui->invoke_show_error(slint::SharedString(msg));
                });
            }
        }

        while (!del_reqs.empty()) {
            mt::remove_request req{};
            del_reqs >> req;
            for (auto const &h: ses.get_torrents()) {
                if (h.id() == req.id) {
                    ses.remove_torrent(h);
                    break;
                }
            }
        }

        while (!create_reqs.empty()) {
            mt::create_request req{};
            create_reqs >> req;

            try {
                mt::create_torrent(req.folder, req.save_path, req.tracker_url);
            } catch (std::exception &e) {
                slint::SharedString msg(e.what());
                slint::invoke_from_event_loop([msg, &ui_weak]() {
                    auto ui = ui_weak.lock();
                    ui.value()->invoke_show_error(msg);
                });
            }
        }

        while (!blacklist_updates.empty()) {
            mt::update_blacklist_request req;
            blacklist_updates >> req;

            std::vector<slint::SharedString> new_blocklist;
            lt::ip_filter filter = ses.get_ip_filter();


            if (req.action == mt::BlacklistUpdate::Add) {

                lt::address address = boost::asio::ip::make_address(std::string(req.ip));

                filter.add_rule(address, address, lt::ip_filter::blocked);

                ses.set_ip_filter(filter);
            } else if (req.action == mt::BlacklistUpdate::Remove) {

            }
            auto ranges = filter.export_filter();
            auto ipv4 = std::get<0>(ranges);
            auto ipv6 = std::get<1>(ranges);

            for (const auto &range: ipv4) {
                if (range.flags == lt::ip_filter::blocked) {
                    new_blocklist.emplace_back(range.first.to_string());
                    if (range.first != range.last) {
                        new_blocklist.emplace_back(range.last.to_string());
                    }
                }
            }
            for (const auto &range: ipv6) {
                if (range.flags == lt::ip_filter::blocked) {
                    new_blocklist.emplace_back(range.first.to_string());
                    if (range.first != range.last) {
                        new_blocklist.emplace_back(range.last.to_string());
                    }
                }
            }

            slint::invoke_from_event_loop([new_blocklist, &ui_weak]() {
                auto blacklist = std::make_shared<slint::VectorModel<slint::SharedString>>();
                blacklist->set_vector(new_blocklist);
                auto ui = ui_weak.lock();
                ui.value()->set_blocked_peers(blacklist);
            });
        }

        // handle the alerts
        for (lt::alert const *a: alerts) {
            // update UI with added torrent
            if (auto at = lt::alert_cast<lt::add_torrent_alert>(a)) {
                TorrentInfo new_info;
                new_info.name = at->torrent_name();
                new_info.ses_id = at->handle.id();
                // we can't get the progress at this stage, so initialise it to 0
                new_info.progress = 0;

                slint::invoke_from_event_loop([new_info, &infos, &ui_weak]() {
                    infos->push_back(new_info);
                    auto ui = ui_weak.lock();
                    ui.value()->set_torrents(infos);
                });
            }

            // update ui to remove torrent
            if (auto alert = lt::alert_cast<lt::torrent_removed_alert>(a)) {
                std::string name = alert->torrent_name();
                // remove the torrent from our lists
                for (int i = 0; i < infos->row_count(); i++) {
                    TorrentInfo info = *infos->row_data(i);
                    if (std::string(info.name) == name) {
                        // update the ui
                        slint::invoke_from_event_loop([i, infos, &ui_weak]() {
                            infos->erase(i);
                            auto ui = *ui_weak.lock();
                            ui->set_torrents(infos);
                        });
                    }
                }

                // delete its remove file
                std::string resume_file_path =
                        mt::storage_dir() + "/resume-files/" + name + ".resume_file";
                if (std::filesystem::exists(resume_file_path)) {
                    std::filesystem::remove(resume_file_path);
                }
            }

            // if a torrent finishes, save its resume data
            if (auto alert = lt::alert_cast<lt::torrent_finished_alert>(a)) {
                if (alert->handle.in_session())
                    alert->handle.save_resume_data(lt::torrent_handle::only_if_modified |
                                                   lt::torrent_handle::save_info_dict);
            }

            // if we receive an error, display it
            if (auto alert = lt::alert_cast<lt::torrent_error_alert>(a)) {
                lt::torrent_handle h = alert->handle;
                std::cout << a->message() << std::endl;
                slint::SharedString msg = slint::SharedString(a->message());
                slint::invoke_from_event_loop([msg, &ui_weak]() {
                    auto ui = *ui_weak.lock();
                    ui->invoke_show_error(msg);
                });
                h.save_resume_data(lt::torrent_handle::only_if_modified |
                                   lt::torrent_handle::save_info_dict);
            }

            // when resume data is ready, save it
            if (auto rd = lt::alert_cast<lt::save_resume_data_alert>(a)) {
                if (rd->handle.in_session())
                    mt::save_torrent_data(rd);
                if (done) goto done;
            }

            if (lt::alert_cast<lt::save_resume_data_failed_alert>(a)) {
                if (done) goto done;
            }

            if (auto st = lt::alert_cast<lt::state_update_alert>(a)) {
                if (st->status.empty()) continue;
                auto peers = std::make_shared<slint::VectorModel<slint::SharedString>>();

                for (auto const &s: st->status) {
                    // print to console for debugging
                    std::cout << '\r' << s.name << ": " << mt::state(s.state) << ' '
                              << (s.download_payload_rate / 1000) << " kB/s "
                              << (s.total_done / 1000) << " kB ("
                              << (s.progress_ppm / 10000) << "%) downloaded ("
                              << s.num_peers << " peers)\x1b[K";
                    std::cout.flush();

                    if (!(s.handle.is_valid() && s.handle.in_session())) {
                        break;
                    }
                    std::vector<lt::peer_info> these_peers;
                    s.handle.get_peer_info(these_peers);
                    // we're only allowed to edit the overall peer list from slint's event loop, so do that
                    slint::invoke_from_event_loop([these_peers, peers]() {
                        for (const auto &p: these_peers) {
                            auto ip = slint::SharedString(p.ip.address().to_string());
                            peers->push_back(ip);
                        }
                    });

                    auto id = s.handle.id();
                    for (int i = 0; i < infos->row_count(); i++) {
                        auto info = *infos->row_data(i);
                        if (info.ses_id == id) {
                            // in case the name has changed, update it here
                            info.name = s.name;
                            info.progress = s.progress;

                            slint::invoke_from_event_loop([i, info, infos, &ui_weak]() {
                                infos->set_row_data(i, info);
                                auto ui = *ui_weak.lock();
                                ui->set_torrents(infos);
                            });
                            break;
                        }
                    }
                }
                // update the UI's peer list
                slint::invoke_from_event_loop([peers, &ui_weak]() {
                    auto ui = *ui_weak.lock();
                    ui->set_peers(peers);
                });
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        // ask the session to post a state_update_alert, to update our
        // state output for the torrent
        ses.post_torrent_updates();

        // save resume data once every 5 seconds
        if (clk::now() - last_save_resume > std::chrono::seconds(5)) {
            for (auto const &h: ses.get_torrents()) {
                if (h.is_valid() && h.in_session()) {
                    h.save_resume_data(lt::torrent_handle::only_if_modified |
                                       lt::torrent_handle::save_info_dict);
                }
            }
            last_save_resume = clk::now();
        }

        if (done) goto done;
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
    msd::channel<mt::create_request> create_channel;
    msd::channel<mt::update_blacklist_request> block_channel;

    // set up request callbacks
    ui->on_add_torrent([&](const auto &torrent, const auto &save_path) {
        mt::add_request req{
                mt::sanitise_path(torrent),
                mt::sanitise_path(save_path)
        };

        add_channel << req;
    });
    ui->on_remove_torrent([&](const auto &id) {
        mt::remove_request req{id};

        remove_channel << req;
    });
    ui->on_create_torrent([&](const auto &folder, const auto &save_path, const auto &tracker_url) {
        mt::create_request req{
                mt::sanitise_path(folder),
                mt::sanitise_path(save_path),
                mt::sanitise_path(tracker_url),
        };
        create_channel << req;
    });
    ui->on_block_ip([&](const auto &ip) {
        mt::update_blacklist_request req(std::string(ip), mt::BlacklistUpdate::Add);
        block_channel << req;
    });

    slint::ComponentWeakHandle<MainWindow> ui_weak(ui);

    std::thread event_thread{
            [ui_weak, &ses, &last_save_resume, &add_channel, &remove_channel, &create_channel, &block_channel]() {
                event_loop(ses, last_save_resume, ui_weak, add_channel, remove_channel, create_channel, block_channel);
            }};


    ui->run();

    // upon returning, the window has been closed so we need to stop the event loop
    shut_down = true;
    event_thread.join();
} catch (std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
}
