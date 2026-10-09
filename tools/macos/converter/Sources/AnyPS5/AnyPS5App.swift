import AppKit
import SwiftUI

@main
struct AnyPS5App: App {
    @NSApplicationDelegateAdaptor(AppDelegate.self) private var delegate
    @State private var model = Model()

    var body: some Scene {
        Window("AnyPS5", id: "main") {
            RootView(model: model)
                .onReceive(NotificationCenter.default.publisher(for: AppDelegate.openFolder)) { note in
                    guard let url = note.object as? URL else { return }
                    model.open(url)
                }
        }
        .windowStyle(.hiddenTitleBar)
        .windowResizability(.contentSize)
    }
}

final class AppDelegate: NSObject, NSApplicationDelegate {
    static let openFolder = Notification.Name("AnyPS5OpenFolder")

    // A game folder dropped on the Dock icon or passed to `open -a AnyPS5`.
    func application(_ application: NSApplication, open urls: [URL]) {
        guard let url = urls.first else { return }
        NotificationCenter.default.post(name: AppDelegate.openFolder, object: url)
    }

    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool {
        true
    }
}
