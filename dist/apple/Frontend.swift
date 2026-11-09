// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2026 the ScarletNX contributors


import Foundation

#if os(iOS)
import UIKit
import UniformTypeIdentifiers


public typealias SNXPicked = @convention(c) (UnsafeMutableRawPointer?, UnsafePointer<CChar>?) -> Void


private func presenter(_ window: UnsafeMutableRawPointer) -> UIViewController? {
    var top = Unmanaged<UIWindow>.fromOpaque(window).takeUnretainedValue().rootViewController
    while let shown = top?.presentedViewController { top = shown }
    return top
}


private final class Picker: NSObject, UIDocumentPickerDelegate {
    static var showing: Picker?
    let done: SNXPicked
    let data: UnsafeMutableRawPointer?

    init(done: SNXPicked, data: UnsafeMutableRawPointer?) {
        self.done = done
        self.data = data
    }

    func documentPicker(_ controller: UIDocumentPickerViewController, didPickDocumentsAt urls: [URL]) {

        guard let url = urls.first else { return finish(nil) }
        _ = url.startAccessingSecurityScopedResource()
        finish(url.path)
    }

    func documentPickerWasCancelled(_ controller: UIDocumentPickerViewController) { finish(nil) }

    private func finish(_ path: String?) {
        if let path {
            path.withCString { done(data, $0) }
        } else {
            done(data, nil)
        }
        Picker.showing = nil
    }
}

@_cdecl("snx_apple_pick")
public func snxApplePick(_ window: UnsafeMutableRawPointer, _ folder: Bool, _ done: SNXPicked,
                         _ data: UnsafeMutableRawPointer?) -> Bool {
    guard Picker.showing == nil, let top = presenter(window) else { return false }

    let picker = UIDocumentPickerViewController(forOpeningContentTypes: folder ? [.folder] : [.item], asCopy: false)
    let delegate = Picker(done: done, data: data)
    picker.delegate = delegate
    picker.allowsMultipleSelection = false
    Picker.showing = delegate
    top.present(picker, animated: true)
    return true
}

@_cdecl("snx_apple_share")
public func snxAppleShare(_ window: UnsafeMutableRawPointer, _ path: UnsafePointer<CChar>) -> Bool {
    guard let top = presenter(window) else { return false }
    let sheet = UIActivityViewController(activityItems: [URL(fileURLWithPath: String(cString: path))],
                                         applicationActivities: nil)

    if let popover = sheet.popoverPresentationController {
        popover.sourceView = top.view
        popover.sourceRect = CGRect(x: top.view.bounds.midX, y: top.view.bounds.midY, width: 0, height: 0)
        popover.permittedArrowDirections = []
    }
    top.present(sheet, animated: true)
    return true
}

#elseif os(macOS)
import AppKit


@_cdecl("snx_apple_pick")
public func snxApplePick(_ window: UnsafeMutableRawPointer, _ folder: Bool,
                         _ done: @convention(c) (UnsafeMutableRawPointer?, UnsafePointer<CChar>?) -> Void,
                         _ data: UnsafeMutableRawPointer?) -> Bool {
    false
}


@_cdecl("snx_apple_share")
public func snxAppleShare(_ window: UnsafeMutableRawPointer, _ path: UnsafePointer<CChar>) -> Bool {
    NSWorkspace.shared.activateFileViewerSelecting([URL(fileURLWithPath: String(cString: path))])
    return true
}
#endif
