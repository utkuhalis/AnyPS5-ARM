import AppKit
import Foundation
import Observation

enum Step: Int, CaseIterable {
    case mac
    case game
    case convert
    case play

    var title: String {
        switch self {
        case .mac: return L.stepMac
        case .game: return L.stepGame
        case .convert: return L.stepConvert
        case .play: return L.stepPlay
        }
    }
}

@MainActor
@Observable
final class Model {
    var step: Step = .mac
    var forward = true

    let appleSilicon: Bool = {
        var value: Int32 = 0
        var size = MemoryLayout<Int32>.size
        return sysctlbyname("hw.optional.arm64", &value, &size, nil, 0) == 0 && value == 1
    }()
    let toolkit = Toolkit.bundled
    var rosettaInstalled = Rosetta.installed
    var installingRosetta = false
    var rosettaFailed = false

    var game: GameFolder?
    var inspecting = false
    var folderError: String?

    var destination: URL = Model.defaultDestination
    var converting = false
    var phase: Phase?
    var finished: Set<Phase> = []
    var log: [String] = []
    var failure: String?
    var result: URL?

    var macReady: Bool { appleSilicon && rosettaInstalled && toolkit != nil }

    static var defaultDestination: URL {
        let applications = URL(fileURLWithPath: "/Applications")
        if FileManager.default.isWritableFile(atPath: applications.path) { return applications }
        let home = FileManager.default.homeDirectoryForCurrentUser.appendingPathComponent("Applications")
        try? FileManager.default.createDirectory(at: home, withIntermediateDirectories: true)
        return home
    }

    func go(_ next: Step) {
        forward = next.rawValue > step.rawValue
        step = next
    }

    func installRosetta() {
        installingRosetta = true
        rosettaFailed = false
        Task.detached {
            let installed = Rosetta.install()
            await MainActor.run {
                self.installingRosetta = false
                self.rosettaInstalled = installed
                self.rosettaFailed = !installed
            }
        }
    }

    func choose(_ url: URL) {
        inspecting = true
        folderError = nil
        Task.detached {
            let outcome = Result { try GameInspector.inspect(url) }
            await MainActor.run {
                self.inspecting = false
                switch outcome {
                case .success(let game):
                    self.game = game
                case .failure(let error):
                    self.game = nil
                    self.folderError = error.localizedDescription
                }
            }
        }
    }

    func open(_ url: URL) {
        guard macReady, !converting else { return }
        if step != .game { go(.game) }
        choose(url)
    }

    func pickFolder() {
        let panel = NSOpenPanel()
        panel.canChooseDirectories = true
        panel.canChooseFiles = false
        panel.allowsMultipleSelection = false
        panel.prompt = L.chooseFolder.replacingOccurrences(of: "…", with: "")
        if panel.runModal() == .OK, let url = panel.url { choose(url) }
    }

    func pickDestination() {
        let panel = NSOpenPanel()
        panel.canChooseDirectories = true
        panel.canChooseFiles = false
        panel.canCreateDirectories = true
        panel.directoryURL = destination
        if panel.runModal() == .OK, let url = panel.url { destination = url }
    }

    func convert() {
        guard let game, let toolkit else { return }
        converting = true
        failure = nil
        finished = []
        phase = nil
        log = []
        let destination = self.destination
        Task.detached {
            let converter = Converter(game: game, destination: destination, toolkit: toolkit, report: { phase in
                Task { @MainActor in
                    if let current = self.phase { self.finished.insert(current) }
                    self.phase = phase
                }
            }, log: { line in
                Task { @MainActor in self.log.append(line) }
            })
            let outcome = Result { try converter.run() }
            await MainActor.run {
                self.converting = false
                switch outcome {
                case .success(let url):
                    if let current = self.phase { self.finished.insert(current) }
                    self.phase = nil
                    self.result = url
                    self.go(.play)
                case .failure(let error):
                    self.failure = error.localizedDescription
                }
            }
        }
    }

    func play() {
        guard let result else { return }
        NSWorkspace.shared.openApplication(at: result, configuration: NSWorkspace.OpenConfiguration())
    }

    func reveal() {
        guard let result else { return }
        NSWorkspace.shared.activateFileViewerSelecting([result])
    }

    func reset() {
        game = nil
        folderError = nil
        result = nil
        finished = []
        phase = nil
        log = []
        failure = nil
        go(.game)
    }
}
