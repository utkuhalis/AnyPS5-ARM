// swift-tools-version:5.9
import PackageDescription

let package = Package(
    name: "AnyPS5",
    platforms: [.macOS(.v14)],
    targets: [
        .executableTarget(name: "AnyPS5", path: "Sources/AnyPS5")
    ]
)
