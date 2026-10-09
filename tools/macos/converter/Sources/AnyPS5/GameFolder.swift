import AppKit
import Foundation

enum ExecutableKind: Equatable {
    case elf
    case selfWithBackup
    case signedOnly
    case missing
}

struct GameFolder: Equatable {
    let url: URL
    let titleID: String
    let name: String
    let version: String
    let icon: NSImage?
    let executable: URL?
    let executableKind: ExecutableKind
    let modules: [URL]
    let missingModules: [String]
    let bytes: Int64

    var convertible: Bool {
        executable != nil && missingModules.isEmpty
    }

    var bundleIdentifier: String {
        "org.anyps5." + titleID.lowercased().filter { $0.isLetter || $0.isNumber }
    }

    static func == (left: GameFolder, right: GameFolder) -> Bool {
        left.url == right.url
    }
}

enum FolderProblem: LocalizedError {
    case noParameters
    case unreadableParameters

    var errorDescription: String? {
        switch self {
        case .noParameters: return L.noParameters
        case .unreadableParameters: return L.unreadableParameters
        }
    }
}

enum Elf {
    static func isElf(_ url: URL) -> Bool {
        guard let handle = try? FileHandle(forReadingFrom: url) else { return false }
        defer { try? handle.close() }
        return handle.readData(ofLength: 4) == Data([0x7f, 0x45, 0x4c, 0x46])
    }

    // A dump whose executable was re-signed keeps the original ELF as <name>.esbak.
    static func resolve(_ url: URL) -> URL? {
        if isElf(url) { return url }
        let backup = url.appendingPathExtension("esbak")
        return isElf(backup) ? backup : nil
    }
}

enum GameInspector {
    static let moduleDirectories = ["sce_module", "sce_modules", "prx"]

    static func inspect(_ url: URL) throws -> GameFolder {
        let fileManager = FileManager.default
        let parametersURL = url.appendingPathComponent("sce_sys/param.json")
        guard fileManager.fileExists(atPath: parametersURL.path) else { throw FolderProblem.noParameters }
        guard let data = try? Data(contentsOf: parametersURL),
              let parameters = try? JSONSerialization.jsonObject(with: data) as? [String: Any] else {
            throw FolderProblem.unreadableParameters
        }
        let titleID = parameters["titleId"] as? String ?? "UNKNOWN"
        let localized = parameters["localizedParameters"] as? [String: Any] ?? [:]
        let language = localized["defaultLanguage"] as? String ?? parameters["defaultLanguage"] as? String ?? "en-US"
        let names = (localized[language] as? [String: Any]) ?? (localized["en-US"] as? [String: Any]) ?? [:]
        let name = names["titleName"] as? String ?? titleID
        let version = parameters["contentVersion"] as? String ?? "1.0"
        let icon = NSImage(contentsOf: url.appendingPathComponent("sce_sys/icon0.png"))

        let eboot = url.appendingPathComponent("eboot.bin")
        let executable = Elf.resolve(eboot)
        let kind: ExecutableKind
        if !fileManager.fileExists(atPath: eboot.path) {
            kind = .missing
        } else if executable == eboot {
            kind = .elf
        } else if executable != nil {
            kind = .selfWithBackup
        } else {
            kind = .signedOnly
        }

        var modules: [URL] = []
        var missing: [String] = []
        for directory in moduleDirectories {
            let folder = url.appendingPathComponent(directory)
            guard let entries = try? fileManager.contentsOfDirectory(at: folder, includingPropertiesForKeys: nil) else { continue }
            for entry in entries.sorted(by: { $0.lastPathComponent < $1.lastPathComponent }) {
                let fileName = entry.lastPathComponent
                guard fileName.hasSuffix(".prx") || fileName.hasSuffix(".sprx") else { continue }
                if Elf.resolve(entry) != nil {
                    modules.append(entry)
                } else {
                    missing.append(directory + "/" + fileName)
                }
            }
        }

        return GameFolder(url: url, titleID: titleID, name: name, version: version, icon: icon, executable: executable, executableKind: kind, modules: modules, missingModules: missing, bytes: size(of: url))
    }

    static func size(of url: URL) -> Int64 {
        guard let enumerator = FileManager.default.enumerator(at: url, includingPropertiesForKeys: [.totalFileAllocatedSizeKey, .isRegularFileKey]) else { return 0 }
        var total: Int64 = 0
        for case let file as URL in enumerator {
            let values = try? file.resourceValues(forKeys: [.totalFileAllocatedSizeKey, .isRegularFileKey])
            if values?.isRegularFile == true { total += Int64(values?.totalFileAllocatedSize ?? 0) }
        }
        return total
    }
}
