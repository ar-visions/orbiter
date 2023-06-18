#pragma once

#include <mx/mx.hpp>
#include <async/async.hpp>
#include <net/net.hpp>

namespace ion {

struct station:mx {
    enums(command, none, 
        "none, project, resource, dependency, ping, pong, repo, del, products, sync-inbound, sync-packet, sync-complete, delivery-inbound, delivery-packet, delivery-complete, auto-build, force-build, force-clean",
         none, project, resource, dependency, ping, pong, repo, del, products, sync_inbound, sync_packet, sync_complete, delivery_inbound, delivery_packet, delivery_complete, auto_build, force_build, force_clean
    );

    /* descriptions
        None,       Project,    /// operands: project, version
        Ping,       Pong,       /// operands: ping and pong have never gone up in smoke, shame
        Activity,               /// operands: simple string used for logging; could potentially have a sub-component name or symbol
        Repo,                   /// operands: module-name, version, uri
        Del,                    /// operands: resource-path
        Products,               /// operands: tells server which to auto-build; target:os:arch target2:os:arch
        
        SyncInbound,            /// operands: transfer of resource described
        SyncPacket,             /// operands: transfer of resource
        SyncComplete,           /// operands: indication transfer is done, auto-build
        
        DeliveryInbound,        /// operands: transfer start indication for a product
        DeliveryPacket,         /// operands: general delivery method, nothing but product path (path relative to products/., not allowed behind it)
        DeliveryComplete,       /// operands: indication that transfer is complete
        
        AutoBuild,              /// operands: this is off until set; part of a startup sequence
        ForceBuild, ForceClean  /// operands: everyone knows these, but force imples implicit else-where
    */

    /// send message
    static bool send(sock sc, station::command com, mx content) {
        static std::mutex mx;
        mx.lock();
        message msg = message(com.symbol(), content);
        bool   sent = msg.write(sc);
        mx.unlock();
        return sent;
    };
    
    /// recv message
    static message recv(sock sc) { return message(sc); };
};
}