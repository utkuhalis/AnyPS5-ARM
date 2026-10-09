import Foundation

enum L {
    static let turkish = Locale.preferredLanguages.first?.hasPrefix("tr") == true

    private static func t(_ english: String, _ turkish: String) -> String {
        self.turkish ? turkish : english
    }

    static var stepMac: String { t("This Mac", "Bu Mac") }
    static var stepGame: String { t("Game", "Oyun") }
    static var stepConvert: String { t("Convert", "Dönüştür") }
    static var stepPlay: String { t("Play", "Oyna") }

    static var checkTitle: String { t("Let's check this Mac", "Önce bu Mac'e bakalım") }
    static var checkSubtitle: String { t("PS5 games run their own x86-64 code under Rosetta 2 and draw through Metal.", "PS5 oyunları kendi x86-64 kodlarını Rosetta 2 altında çalıştırır ve Metal ile çizer.") }
    static var appleSilicon: String { t("Apple silicon", "Apple silicon") }
    static var appleSiliconMissing: String { t("This Mac has an Intel processor; AnyPS5-ARM is made for Apple silicon.", "Bu Mac'te Intel işlemci var; AnyPS5-ARM, Apple silicon için yapıldı.") }
    static var rosetta: String { t("Rosetta 2", "Rosetta 2") }
    static var rosettaMissing: String { t("Not installed yet. It takes a minute and needs your password.", "Henüz kurulu değil. Bir dakika sürer ve şifreni ister.") }
    static var rosettaInstall: String { t("Install Rosetta", "Rosetta'yı kur") }
    static var rosettaInstalling: String { t("Installing…", "Kuruluyor…") }
    static var rosettaFailed: String { t("Rosetta could not be installed. Try again, or run softwareupdate --install-rosetta in Terminal.", "Rosetta kurulamadı. Tekrar dene ya da Terminal'de softwareupdate --install-rosetta çalıştır.") }
    static var ready: String { t("Ready", "Hazır") }
    static var toolkit: String { t("Conversion tools", "Dönüştürme araçları") }
    static var toolkitMissing: String { t("This copy of AnyPS5 has no bundled tools. Download it again from the releases page.", "Bu AnyPS5 kopyasında araçlar yok. Releases sayfasından tekrar indir.") }
    static var continueLabel: String { t("Continue", "Devam") }
    static var back: String { t("Back", "Geri") }

    static var chooseTitle: String { t("Choose your game", "Oyununu seç") }
    static var chooseSubtitle: String { t("Pick the folder of a decrypted PS5 game dump you own: the one with eboot.bin and sce_sys inside.", "Sahip olduğun, şifresi çözülmüş bir PS5 oyun dump'ının klasörünü seç: içinde eboot.bin ve sce_sys olan klasör.") }
    static var dropHere: String { t("Drop the game folder here", "Oyun klasörünü buraya bırak") }
    static var orChoose: String { t("or", "ya da") }
    static var chooseFolder: String { t("Choose Folder…", "Klasör Seç…") }
    static var chooseAnother: String { t("Choose Another…", "Başka Klasör Seç…") }
    static var noParameters: String { t("This folder has no sce_sys/param.json. Choose the game's top folder (the one that contains eboot.bin).", "Bu klasörde sce_sys/param.json yok. Oyunun en üst klasörünü seç (içinde eboot.bin olan).") }
    static var unreadableParameters: String { t("sce_sys/param.json could not be read.", "sce_sys/param.json okunamadı.") }
    static var executable: String { t("Executable", "Çalıştırılabilir dosya") }
    static var executableElf: String { t("Decrypted ELF", "Şifresi çözülmüş ELF") }
    static var executableBackup: String { t("Signed eboot.bin; its decrypted backup (.esbak) is used", "eboot.bin imzalı; şifresi çözülmüş yedeği (.esbak) kullanılacak") }
    static var executableSigned: String { t("eboot.bin is still signed or encrypted. AnyPS5 needs a decrypted dump.", "eboot.bin hâlâ imzalı ya da şifreli. AnyPS5 şifresi çözülmüş bir dump istiyor.") }
    static var executableMissing: String { t("There is no eboot.bin in this folder.", "Bu klasörde eboot.bin yok.") }
    static var modules: String { t("Bundled modules", "Oyunun modülleri") }
    static func modulesFound(_ count: Int) -> String { t("\(count) ready", "\(count) hazır") }
    static var modulesNone: String { t("None", "Yok") }
    static func modulesMissing(_ names: String) -> String { t("Still encrypted: \(names)", "Hâlâ şifreli: \(names)") }

    static var convertTitle: String { t("Make it a Mac app", "Mac uygulamasına dönüştür") }
    static var convertSubtitle: String { t("AnyPS5 relinks the game and packs it with everything it needs into one app.", "AnyPS5 oyunu yeniden bağlar ve ihtiyaç duyduğu her şeyle birlikte tek bir uygulamaya paketler.") }
    static var saveTo: String { t("Save to", "Kayıt yeri") }
    static var change: String { t("Change…", "Değiştir…") }
    static var convert: String { t("Convert", "Dönüştür") }
    static var converting: String { t("Converting…", "Dönüştürülüyor…") }
    static var showLog: String { t("Details", "Ayrıntılar") }
    static var tryAgain: String { t("Try Again", "Tekrar Dene") }
    static var failedTitle: String { t("The conversion stopped", "Dönüştürme durdu") }
    static var phasePrepare: String { t("Reading the game files", "Oyun dosyaları okunuyor") }
    static var phaseRelink: String { t("Relinking for macOS", "macOS için yeniden bağlanıyor") }
    static var phaseCopy: String { t("Copying the game data", "Oyun verileri kopyalanıyor") }
    static var phaseBundle: String { t("Adding system libraries and Metal driver", "Sistem kütüphaneleri ve Metal sürücüsü ekleniyor") }
    static var phaseFinish: String { t("Finishing the app", "Uygulama tamamlanıyor") }
    static var noExecutable: String { t("No decrypted executable was found.", "Şifresi çözülmüş bir çalıştırılabilir dosya bulunamadı.") }
    static func toolFailed(_ tool: String, _ status: Int, _ last: String) -> String {
        t("\(tool) failed (\(status)). \(last)", "\(tool) başarısız oldu (\(status)). \(last)")
    }

    static var doneTitle: String { t("Ready to play", "Oynamaya hazır") }
    static func doneSubtitle(_ name: String) -> String { t("\(name) is now a Mac app. Open it like any other app, from Finder or Launchpad.", "\(name) artık bir Mac uygulaması. Diğer uygulamalar gibi Finder'dan ya da Launchpad'den aç.") }
    static var play: String { t("Play", "Oyna") }
    static var showInFinder: String { t("Show in Finder", "Finder'da Göster") }
    static var convertAnother: String { t("Convert Another Game", "Başka Oyun Dönüştür") }
    static var firstRunNote: String { t("The first launch compiles shaders and may stutter; later launches load them from the cache. Controls: arrow keys, Return (Cross), Escape (Options), or any controller.", "İlk açılışta shader'lar derlendiği için takılmalar olabilir; sonraki açılışlar önbellekten yükler. Kontroller: yön tuşları, Return (Çarpı), Escape (Options) ya da herhangi bir kol.") }
}
