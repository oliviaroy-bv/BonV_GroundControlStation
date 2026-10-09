const exeNmae = "BonVGroundStation";

function Component() {
    // Default Component constructor
}

// Creates installation operations and adds shortcuts for Windows.
Component.prototype.createOperations = function() {

    // delayInstallationForPython(0.1);

    // Call default implementation to install binaries
    component.createOperations();

    // Add shortcuts for Windows
    if (systemInfo.productType === "windows") {
        component.addOperation(
            "CreateShortcut",
            "@TargetDir@/" + exeNmae + ".exe",
            "@StartMenuDir@/" + exeNmae + ".lnk",
            "workingDirectory=@TargetDir@",
            "iconPath=@TargetDir@/" + exeNmae + ".exe",
            "description=@Description@"
        );

        var installPath = installer.value("TargetDir");
        var version = installer.value("Version");

        // console.log("path: " + installPath);
        // console.log("Version: " + version);

        addRegistryEntries(installPath, version);
        console.log("Already Executed");
    }
};

// Adds registry entries for the installation path and version.
//function addRegistryEntries(installPath, version) {
//    try {
//        var cmd = installer.environmentVariable("SystemRoot") + "\\System32\\cmd.exe";

        // Define registry commands
//        var companyKey = "HKCU\\Software\\BonV";
//        var softwareKey = companyKey + "\\" + exeNmae;
//        var addCompanyKey = "reg add " + companyKey + " /f";
//        var addSoftwareKey = "reg add " + softwareKey + " /f";
//        var addInstallPath = "reg add " + softwareKey + " /v InstallationPath /t REG_SZ /d \"" + installPath + "\" /f";
//        var addVersion = "reg add " + softwareKey + " /v Version /t REG_SZ /d \"" + version + "\" /f";

        // Undo operation to remove the registry keys
//        var deleteSoftwareKey = "reg delete " + softwareKey + " /f";
        // var deleteCompanyKey = "reg delete " + companyKey + " /f";

        // Execute commands
        // console.log("Adding Company Key");
//        component.addOperation("Execute", cmd, "/C", addCompanyKey);
        // console.log("Adding Software Key");
//        component.addOperation("Execute", cmd, "/C", addSoftwareKey, "UNDOEXECUTE", cmd, "/C", deleteSoftwareKey);
        // console.log("Adding Installation Path");
//        component.addOperation("Execute", cmd, "/C", addInstallPath);
        // console.log("Adding Version");
//        component.addOperation("Execute", cmd, "/C", addVersion);

        
        // component.addOperation("Execute", cmd, "/C", deleteSoftwareKey, "UNDOEXECUTE", cmd, "/C", deleteSoftwareKey);
        // component.addOperation("Execute", cmd, "/C", deleteCompanyKey, "UNDOEXECUTE", cmd, "/C", deleteCompanyKey);

//    } catch (e) {
//        print(e);
//    }
//}

function addRegistryEntries(installPath, version)
{
    try
    {
        var regExe = installer.environmentVariable("SystemRoot") + "\\System32\\reg.exe";

        var companyKey  = "HKCU\\Software\\BonV";
        var softwareKey = companyKey + "\\" + exeNmae;

        console.log("Installation Path : " + installPath);
        console.log("Version           : " + version);

        // Create Company Key
        component.addOperation(
            "Execute",
            regExe,
            "add",
            companyKey,
            "/f"
        );

        // Create Software Key
        component.addOperation(
            "Execute",
            regExe,
            "add",
            softwareKey,
            "/f",
            "UNDOEXECUTE",
            regExe,
            "delete",
            softwareKey,
            "/f"
        );

        // Store Installation Path
        component.addOperation(
            "Execute",
            regExe,
            "add",
            softwareKey,
            "/v", "InstallationPath",
            "/t", "REG_SZ",
            "/d", installPath,
            "/f"
        );

        // Store Version
        component.addOperation(
            "Execute",
            regExe,
            "add",
            softwareKey,
            "/v", "Version",
            "/t", "REG_SZ",
            "/d", version,
            "/f"
        );
    }
    catch (e)
    {
        console.log(e);
    }
}

// function sleep(ms) {
//     // return new Promise(resolve => setTimeout(resolve, ms));

//     for(var i=0; i <= ms*100; i++);
// }

// function delayMinutes(minutes)
// {
//     sleep(minutes * 60 * 1000);
// }

function delayInstallationForPython(minutes) {
    console.log("Holding installation for Python installation...");
    
    // Show a message to the user while delaying
    installer.setValue("ProgressLabelText", "Waiting for Python installation to complete. Please wait...");

    var start = new Date().getTime();
    while (new Date().getTime() - start < (minutes * 60 * 1000)) {
    }

    console.log("Delay complete. Continuing with installation...");
}
