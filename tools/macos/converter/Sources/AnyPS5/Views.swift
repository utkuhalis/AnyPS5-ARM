import AppKit
import SwiftUI
import UniformTypeIdentifiers

struct RootView: View {
    @Bindable var model: Model

    var body: some View {
        VStack(spacing: 0) {
            Header(step: model.step)
                .padding(.top, 34)
                .padding(.bottom, 18)
                .padding(.horizontal, 32)
            Divider()
            ZStack {
                switch model.step {
                case .mac: MacStep(model: model).transition(slide)
                case .game: GameStep(model: model).transition(slide)
                case .convert: ConvertStep(model: model).transition(slide)
                case .play: PlayStep(model: model).transition(slide)
                }
            }
            .frame(maxWidth: .infinity, maxHeight: .infinity)
            .clipped()
        }
        .frame(width: 660, height: 580)
        .background(.background)
        .animation(.smooth(duration: 0.32), value: model.step)
    }

    private var slide: AnyTransition {
        .asymmetric(
            insertion: .move(edge: model.forward ? .trailing : .leading).combined(with: .opacity),
            removal: .move(edge: model.forward ? .leading : .trailing).combined(with: .opacity)
        )
    }
}

struct Header: View {
    let step: Step

    var body: some View {
        HStack(spacing: 14) {
            Logo(size: 30)
            Text("AnyPS5")
                .font(.system(size: 17, weight: .semibold, design: .rounded))
            Spacer()
            HStack(spacing: 6) {
                ForEach(Step.allCases, id: \.self) { item in
                    StepPill(item: item, current: step)
                    if item != Step.allCases.last {
                        Capsule()
                            .fill(item.rawValue < step.rawValue ? Color.accentColor.opacity(0.5) : Color.secondary.opacity(0.2))
                            .frame(width: 14, height: 2)
                    }
                }
            }
        }
    }
}

struct StepPill: View {
    let item: Step
    let current: Step

    private var done: Bool { item.rawValue < current.rawValue }
    private var active: Bool { item == current }

    var body: some View {
        HStack(spacing: 6) {
            ZStack {
                Circle()
                    .fill(active || done ? Color.accentColor : Color.secondary.opacity(0.15))
                if done {
                    Image(systemName: "checkmark")
                        .font(.system(size: 9, weight: .bold))
                        .foregroundStyle(.white)
                        .transition(.scale.combined(with: .opacity))
                } else {
                    Text("\(item.rawValue + 1)")
                        .font(.system(size: 10, weight: .semibold, design: .rounded))
                        .foregroundStyle(active ? .white : .secondary)
                }
            }
            .frame(width: 18, height: 18)
            Text(item.title)
                .font(.system(size: 12, weight: active ? .semibold : .regular))
                .foregroundStyle(active ? .primary : .secondary)
        }
        .padding(.vertical, 4)
        .padding(.leading, 4)
        .padding(.trailing, 9)
        .background(Capsule().fill(active ? Color.accentColor.opacity(0.12) : .clear))
    }
}

struct Logo: View {
    let size: CGFloat

    var body: some View {
        RoundedRectangle(cornerRadius: size * 0.27, style: .continuous)
            .fill(LinearGradient(colors: [Color(red: 0.28, green: 0.36, blue: 0.98), Color(red: 0.55, green: 0.27, blue: 0.93)], startPoint: .topLeading, endPoint: .bottomTrailing))
            .overlay(
                Image(systemName: "gamecontroller.fill")
                    .font(.system(size: size * 0.5, weight: .semibold))
                    .foregroundStyle(.white)
            )
            .frame(width: size, height: size)
            .shadow(color: Color(red: 0.4, green: 0.3, blue: 0.95).opacity(0.35), radius: size * 0.2, y: size * 0.08)
    }
}

struct StepPage<Content: View, Footer: View>: View {
    let title: String
    let subtitle: String
    @ViewBuilder let content: Content
    @ViewBuilder let footer: Footer

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            VStack(alignment: .leading, spacing: 6) {
                Text(title)
                    .font(.system(size: 24, weight: .semibold))
                Text(subtitle)
                    .font(.system(size: 13))
                    .foregroundStyle(.secondary)
                    .fixedSize(horizontal: false, vertical: true)
            }
            .padding(.bottom, 22)
            content
            Spacer(minLength: 0)
            HStack(spacing: 10) { footer }
                .controlSize(.large)
        }
        .padding(.horizontal, 32)
        .padding(.top, 26)
        .padding(.bottom, 24)
    }
}

struct StatusRow: View {
    enum State { case ok, problem, working, idle }

    let state: State
    let title: String
    let detail: String
    var trailing: AnyView? = nil

    var body: some View {
        HStack(alignment: .center, spacing: 12) {
            Group {
                switch state {
                case .ok:
                    Image(systemName: "checkmark.circle.fill").foregroundStyle(.green)
                case .problem:
                    Image(systemName: "exclamationmark.circle.fill").foregroundStyle(.orange)
                case .working:
                    ProgressView().controlSize(.small)
                case .idle:
                    Image(systemName: "circle").foregroundStyle(.tertiary)
                }
            }
            .font(.system(size: 17))
            .frame(width: 20)
            VStack(alignment: .leading, spacing: 2) {
                Text(title).font(.system(size: 13, weight: .medium))
                Text(detail)
                    .font(.system(size: 12))
                    .foregroundStyle(.secondary)
                    .fixedSize(horizontal: false, vertical: true)
            }
            Spacer(minLength: 8)
            if let trailing { trailing }
        }
        .padding(.vertical, 11)
        .padding(.horizontal, 14)
    }
}

struct Card<Content: View>: View {
    @ViewBuilder let content: Content

    var body: some View {
        VStack(spacing: 0) { content }
            .background(RoundedRectangle(cornerRadius: 12, style: .continuous).fill(Color.primary.opacity(0.035)))
            .overlay(RoundedRectangle(cornerRadius: 12, style: .continuous).strokeBorder(Color.primary.opacity(0.07)))
    }
}

struct MacStep: View {
    @Bindable var model: Model

    var body: some View {
        StepPage(title: L.checkTitle, subtitle: L.checkSubtitle) {
            Card {
                StatusRow(state: model.appleSilicon ? .ok : .problem, title: L.appleSilicon, detail: model.appleSilicon ? L.ready : L.appleSiliconMissing)
                Divider().padding(.leading, 46)
                StatusRow(
                    state: model.installingRosetta ? .working : (model.rosettaInstalled ? .ok : .problem),
                    title: L.rosetta,
                    detail: model.rosettaInstalled ? L.ready : (model.rosettaFailed ? L.rosettaFailed : L.rosettaMissing),
                    trailing: model.rosettaInstalled ? nil : AnyView(
                        Button(model.installingRosetta ? L.rosettaInstalling : L.rosettaInstall) { model.installRosetta() }
                            .disabled(model.installingRosetta || !model.appleSilicon)
                    )
                )
                Divider().padding(.leading, 46)
                StatusRow(state: model.toolkit != nil ? .ok : .problem, title: L.toolkit, detail: model.toolkit != nil ? L.ready : L.toolkitMissing)
            }
        } footer: {
            Spacer()
            Button(L.continueLabel) { model.go(.game) }
                .keyboardShortcut(.defaultAction)
                .disabled(!model.macReady)
        }
    }
}

struct GameStep: View {
    @Bindable var model: Model
    @State private var targeted = false

    var body: some View {
        StepPage(title: L.chooseTitle, subtitle: L.chooseSubtitle) {
            if let game = model.game {
                GameCard(game: game)
                    .transition(.opacity.combined(with: .scale(scale: 0.98)))
            } else {
                dropZone
                    .transition(.opacity)
            }
            if let error = model.folderError {
                Label(error, systemImage: "exclamationmark.triangle.fill")
                    .font(.system(size: 12))
                    .foregroundStyle(.orange)
                    .padding(.top, 12)
                    .transition(.opacity)
            }
        } footer: {
            Button(L.back) { model.go(.mac) }
            Spacer()
            if model.game != nil {
                Button(L.chooseAnother) { model.pickFolder() }
            }
            Button(L.continueLabel) { model.go(.convert) }
                .keyboardShortcut(.defaultAction)
                .disabled(model.game?.convertible != true)
        }
        .animation(.smooth(duration: 0.25), value: model.game)
        .animation(.smooth(duration: 0.25), value: model.folderError)
        .onDrop(of: [.fileURL], isTargeted: $targeted) { providers in
            guard let provider = providers.first else { return false }
            _ = provider.loadObject(ofClass: URL.self) { url, _ in
                guard let url else { return }
                Task { @MainActor in model.choose(url) }
            }
            return true
        }
    }

    private var dropZone: some View {
        VStack(spacing: 12) {
            if model.inspecting {
                ProgressView()
            } else {
                Image(systemName: "folder.badge.plus")
                    .font(.system(size: 34, weight: .light))
                    .foregroundStyle(targeted ? Color.accentColor : .secondary)
                    .symbolEffect(.bounce, value: targeted)
                Text(L.dropHere)
                    .font(.system(size: 14, weight: .medium))
                Text(L.orChoose)
                    .font(.system(size: 12))
                    .foregroundStyle(.tertiary)
                Button(L.chooseFolder) { model.pickFolder() }
                    .controlSize(.large)
            }
        }
        .frame(maxWidth: .infinity)
        .frame(height: 230)
        .background(
            RoundedRectangle(cornerRadius: 16, style: .continuous)
                .fill(targeted ? Color.accentColor.opacity(0.08) : Color.primary.opacity(0.025))
        )
        .overlay(
            RoundedRectangle(cornerRadius: 16, style: .continuous)
                .strokeBorder(targeted ? Color.accentColor : Color.secondary.opacity(0.35), style: StrokeStyle(lineWidth: 1.5, dash: [6, 5]))
        )
        .scaleEffect(targeted ? 1.01 : 1)
        .animation(.smooth(duration: 0.18), value: targeted)
    }
}

struct GameIcon: View {
    let image: NSImage?
    let size: CGFloat

    var body: some View {
        Group {
            if let image {
                Image(nsImage: image).resizable().interpolation(.high).aspectRatio(contentMode: .fill)
            } else {
                Logo(size: size)
            }
        }
        .frame(width: size, height: size)
        .clipShape(RoundedRectangle(cornerRadius: size * 0.22, style: .continuous))
        .shadow(color: .black.opacity(0.18), radius: size * 0.1, y: size * 0.04)
    }
}

struct GameCard: View {
    let game: GameFolder

    var body: some View {
        VStack(alignment: .leading, spacing: 16) {
            HStack(spacing: 16) {
                GameIcon(image: game.icon, size: 76)
                VStack(alignment: .leading, spacing: 4) {
                    Text(game.name)
                        .font(.system(size: 20, weight: .semibold))
                        .lineLimit(1)
                    Text("\(game.titleID)  ·  v\(game.version)  ·  \(ByteCountFormatter.string(fromByteCount: game.bytes, countStyle: .file))")
                        .font(.system(size: 12).monospacedDigit())
                        .foregroundStyle(.secondary)
                    Text(game.url.path.replacingOccurrences(of: NSHomeDirectory(), with: "~"))
                        .font(.system(size: 11))
                        .foregroundStyle(.tertiary)
                        .lineLimit(1)
                        .truncationMode(.middle)
                }
            }
            Card {
                StatusRow(state: executableState, title: L.executable, detail: executableDetail)
                Divider().padding(.leading, 46)
                StatusRow(state: game.missingModules.isEmpty ? .ok : .problem, title: L.modules, detail: modulesDetail)
            }
        }
    }

    private var executableState: StatusRow.State {
        switch game.executableKind {
        case .elf, .selfWithBackup: return .ok
        case .signedOnly, .missing: return .problem
        }
    }

    private var executableDetail: String {
        switch game.executableKind {
        case .elf: return L.executableElf
        case .selfWithBackup: return L.executableBackup
        case .signedOnly: return L.executableSigned
        case .missing: return L.executableMissing
        }
    }

    private var modulesDetail: String {
        if !game.missingModules.isEmpty { return L.modulesMissing(game.missingModules.joined(separator: ", ")) }
        return game.modules.isEmpty ? L.modulesNone : L.modulesFound(game.modules.count)
    }
}

struct ConvertStep: View {
    @Bindable var model: Model
    @State private var showLog = false

    var body: some View {
        StepPage(title: L.convertTitle, subtitle: L.convertSubtitle) {
            VStack(alignment: .leading, spacing: 14) {
                if let game = model.game {
                    HStack(spacing: 12) {
                        GameIcon(image: game.icon, size: 40)
                        VStack(alignment: .leading, spacing: 2) {
                            Text(game.name).font(.system(size: 14, weight: .semibold))
                            HStack(spacing: 4) {
                                Text(L.saveTo + ":")
                                Text(model.destination.path.replacingOccurrences(of: NSHomeDirectory(), with: "~"))
                                    .lineLimit(1)
                                    .truncationMode(.middle)
                            }
                            .font(.system(size: 12))
                            .foregroundStyle(.secondary)
                        }
                        Spacer()
                        Button(L.change) { model.pickDestination() }
                            .disabled(model.converting)
                    }
                }
                Card {
                    ForEach(Phase.allCases) { phase in
                        StatusRow(state: state(of: phase), title: phase.title, detail: "")
                            .padding(.vertical, -4)
                        if phase != Phase.allCases.last { Divider().padding(.leading, 46) }
                    }
                }
                if let failure = model.failure {
                    Label {
                        VStack(alignment: .leading, spacing: 2) {
                            Text(L.failedTitle).font(.system(size: 13, weight: .semibold))
                            Text(failure).font(.system(size: 12)).foregroundStyle(.secondary).textSelection(.enabled)
                        }
                    } icon: {
                        Image(systemName: "exclamationmark.triangle.fill").foregroundStyle(.orange)
                    }
                    .transition(.opacity)
                }
                if !model.log.isEmpty {
                    DisclosureGroup(L.showLog, isExpanded: $showLog) {
                        ScrollView {
                            Text(model.log.suffix(400).joined(separator: "\n"))
                                .font(.system(size: 11, design: .monospaced))
                                .foregroundStyle(.secondary)
                                .frame(maxWidth: .infinity, alignment: .leading)
                                .textSelection(.enabled)
                        }
                        .frame(height: 90)
                    }
                    .font(.system(size: 12))
                }
            }
        } footer: {
            Button(L.back) { model.go(.game) }
                .disabled(model.converting)
            Spacer()
            Button(model.converting ? L.converting : (model.failure == nil ? L.convert : L.tryAgain)) { model.convert() }
                .keyboardShortcut(.defaultAction)
                .disabled(model.converting)
        }
        .animation(.smooth(duration: 0.25), value: model.failure)
    }

    private func state(of phase: Phase) -> StatusRow.State {
        if model.finished.contains(phase) { return .ok }
        if model.phase == phase { return model.failure != nil ? .problem : .working }
        return .idle
    }
}

struct PlayStep: View {
    @Bindable var model: Model
    @State private var appeared = false

    var body: some View {
        VStack(spacing: 0) {
            Spacer()
            GameIcon(image: model.game?.icon, size: 112)
                .scaleEffect(appeared ? 1 : 0.85)
                .opacity(appeared ? 1 : 0)
            Text(L.doneTitle)
                .font(.system(size: 26, weight: .semibold))
                .padding(.top, 22)
            Text(L.doneSubtitle(model.game?.name ?? ""))
                .font(.system(size: 13))
                .foregroundStyle(.secondary)
                .multilineTextAlignment(.center)
                .frame(maxWidth: 420)
                .padding(.top, 6)
            HStack(spacing: 10) {
                Button(L.showInFinder) { model.reveal() }
                Button {
                    model.play()
                } label: {
                    Label(L.play, systemImage: "play.fill").padding(.horizontal, 10)
                }
                .buttonStyle(.borderedProminent)
                .keyboardShortcut(.defaultAction)
            }
            .controlSize(.large)
            .padding(.top, 24)
            Spacer()
            Text(L.firstRunNote)
                .font(.system(size: 11))
                .foregroundStyle(.tertiary)
                .multilineTextAlignment(.center)
                .frame(maxWidth: 470)
            Button(L.convertAnother) { model.reset() }
                .buttonStyle(.link)
                .font(.system(size: 12))
                .padding(.top, 10)
                .padding(.bottom, 22)
        }
        .frame(maxWidth: .infinity)
        .onAppear {
            withAnimation(.spring(response: 0.45, dampingFraction: 0.7).delay(0.1)) { appeared = true }
        }
    }
}
