const exeNmae = "BonVGroundStation";
var alreadyHandledExistingInstall = false;

function Controller() {
    installer.finishButtonClicked.connect(finishedClickedHandler);
    installer.setValue("AllowNonEmptyTargetDir", "true");
    installer.setValue("TargetExists", "overwrite");
    installer.setValue("SkipTargetDirWarning", "true");
}

Controller.prototype.IntroductionPageCallback = function()
{
    console.log("=== IntroductionPageCallback ===");
    console.log("isUpdater: " + installer.isUpdater());
    console.log("isUninstaller: " + installer.isUninstaller());
    console.log("isPackageManager: " + installer.isPackageManager());
    console.log("isInstaller: " + installer.isInstaller());
    console.log("alreadyHandledExistingInstall: " + alreadyHandledExistingInstall);

    // =====================================================================
    // MAINTENANCE TOOL MODE - Auto-proceed with uninstallation
    // =====================================================================
    if (installer.isUninstaller()) {
        console.log("Running in UNINSTALLER mode - will auto-proceed");
        alreadyHandledExistingInstall = false;
        
        // Hide the radio button options
        var widget = gui.currentPageWidget();
        if (widget != null) {
            widget.findChild("PackageManagerRadioButton").visible = false;
            widget.findChild("UpdaterRadioButton").visible = false;
            widget.findChild("UninstallerRadioButton").visible = false;
        }
        
        // Auto-click Next to proceed with uninstall
        gui.clickButton(buttons.NextButton);
        return;
    }
    
    if (installer.isPackageManager()) {
        console.log("Running in PACKAGE MANAGER mode");
        alreadyHandledExistingInstall = false;
        
        var widget = gui.currentPageWidget();
        if (widget != null) {
            // Select uninstaller radio button
            var uninstallRadio = widget.findChild("UninstallerRadioButton");
            if (uninstallRadio) {
                uninstallRadio.checked = true;
            }
            widget.findChild("PackageManagerRadioButton").visible = false;
            widget.findChild("UpdaterRadioButton").visible = false;
        }
        
        // Auto-click Next to proceed
        gui.clickButton(buttons.NextButton);
        return;
    }

    // =====================================================================
    // INSTALLER MODE - Check for existing installation
    // =====================================================================
    if (installer.isInstaller() && !alreadyHandledExistingInstall && isApplicationInstalled()) {
        alreadyHandledExistingInstall = true;
        console.log("Application already installed - need to uninstall first");
        
        var installationInfo = getInstallationInfo();
        if (!installationInfo) {
            QMessageBox.critical(
                "installation.error",
                "Installation Error",
                "Unable to retrieve installation information. Please uninstall manually.",
                QMessageBox.Ok
            );
            gui.rejectWithoutPrompt();
            return;
        }

        // Compare versions
        var versionComparison = compareVersions(installationInfo.version, installer.value("Version"));
        var message = "";
        
        switch(versionComparison) {
            case 1:
                message = installer.value("Name") + " version " + installationInfo.version + 
                         " is already installed (newer than " + installer.value("Version") + 
                         ").\n\nThe existing version will be uninstalled automatically.";
                break;
            case -1:
                message = installer.value("Name") + " version " + installationInfo.version + 
                         " is already installed.\n\nYou are attempting to install version " + 
                         installer.value("Version") + ".\n\nThe existing version will be uninstalled automatically.";
                break;
            case 0:
                message = installer.value("Name") + " version " + installationInfo.version + 
                         " is already installed.\n\nIt will be uninstalled and then reinstalled.";
                break;
        }

        var response = QMessageBox.question(
            "uninstall.required",
            "Uninstall Required",
            message + "\n\nThe maintenance tool will open briefly to complete the uninstallation.\nThen the installer will automatically restart.\n\nDo you want to proceed?",
            QMessageBox.Yes | QMessageBox.No
        );

        if (response === QMessageBox.Yes) {
            console.log("User chose YES - launching maintenance tool for uninstall");
            
            // Save installer path for restart
            if (!addTempInstallerPath()) {
                console.log("Warning: Could not save installer path");
            }
            
            var maintenanceToolPath = installationInfo.path + "/maintenancetool.exe";
            console.log("Launching maintenance tool: " + maintenanceToolPath);
            
            // Launch maintenance tool - it will auto-proceed due to our callbacks above
            var result = installer.executeDetached(maintenanceToolPath, []);
            
            if (!result) {
                console.log("Failed to launch maintenance tool");
                QMessageBox.critical(
                    "error.maintenancetool",
                    "Error",
                    "Failed to open the maintenance tool.\n\nPlease uninstall manually from:\n" + maintenanceToolPath,
                    QMessageBox.Ok
                );
            } else {
                console.log("Maintenance tool launched successfully");
            }
            console.log("Closing installer");
            gui.rejectWithoutPrompt();
            return;
        } else {
            console.log("User chose NO - canceling installation");
            console.log("Closing installer");
            installer.setDefaultPageVisible(QInstaller.Introduction, false);
            installer.setDefaultPageVisible(QInstaller.TargetDirectory, false);
            installer.setDefaultPageVisible(QInstaller.ComponentSelection, false);
            installer.setDefaultPageVisible(QInstaller.LicenseCheck, false);
            installer.setDefaultPageVisible(QInstaller.ReadyForInstallation, false);
            installer.setDefaultPageVisible(QInstaller.PerformInstallation, false);
            installer.setDefaultPageVisible(QInstaller.InstallationFinished, false);
            gui.reject();
            return;
        }
        
        // Close current installer
        
    }
    
    if (!alreadyHandledExistingInstall) {
        console.log("No existing installation found - proceeding with installation");
    }
}

// Auto-proceed through ComponentSelection in uninstaller mode
Controller.prototype.ComponentSelectionPageCallback = function()
{
    if (installer.isUninstaller()) {
        console.log("Auto-proceeding through ComponentSelection (uninstaller)");
        gui.clickButton(buttons.NextButton);
        return;
    }
    
    // Safety check for installer mode
    if (isApplicationInstalled()) {
        QMessageBox.critical(
            "installation.blocked",
            "Installation Error",
            "An existing installation was detected. Please restart the installer.",
            QMessageBox.Ok
        );
        gui.rejectWithoutPrompt();
    }
}

// Auto-proceed through ReadyForInstallation in uninstaller mode
Controller.prototype.ReadyForInstallationPageCallback = function()
{
    if (installer.isUninstaller()) {
        console.log("Auto-proceeding through ReadyForInstallation (uninstaller)");
        gui.clickButton(buttons.CommitButton);
        return;
    }
}

Controller.prototype.TargetDirectoryPageCallback = function() 
{
    var widget = gui.currentPageWidget();
    if (!widget) return;
    widget.allowSpaceInPath = true;
}

// Handle finish button - restart installer after uninstall
function finishedClickedHandler() {
    console.log("=== Finish clicked ===");
    console.log("isUninstaller: " + installer.isUninstaller());
    console.log("isPackageManager: " + installer.isPackageManager());

    if (installer.isUninstaller() || installer.isPackageManager()) {
        console.log("Uninstall finished - restarting installer");

        var newInstaller = tempInstallerPath();
        
        if (newInstaller && newInstaller !== "") {
            console.log("Waiting 2 seconds for cleanup...");
            installer.execute("cmd.exe", ["/c", "timeout", "/t", "2", "/nobreak"]);
            
            console.log("Restarting installer: " + newInstaller);
            removeTempInstallerPath();
            
            var restartResult = installer.executeDetached(newInstaller, []);
            
            if (restartResult) {
                console.log("Installer restarted successfully");
            } else {
                console.log("Failed to restart installer");
                QMessageBox.information(
                    "restart.manual",
                    "Restart Required",
                    "Please run the installer again to complete installation:\n" + newInstaller,
                    QMessageBox.Ok
                );
            }
        } else {
            console.log("No installer path found");
            removeTempInstallerPath();
        }

        gui.rejectWithoutPrompt();
    }
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

function isApplicationInstalled() {
    var companyKey = "HKCU\\Software\\BonV";
    var softwareKey = companyKey + "\\" + exeNmae;
    var result = installer.execute("cmd.exe", ["/c", "reg", "query", softwareKey, "/v", "Version", "2>nul"]);

    if (result[1] === 0) {
        console.log("Application is installed (registry key exists)");
        return true;
    }

    console.log("Application is not installed");
    return false;
}

function getInstallationInfo() {
    var companyKey = "HKCU\\Software\\BonV";
    var softwareKey = companyKey + "\\" + exeNmae;
    var result = installer.execute("cmd.exe", ["/c", "reg", "query", softwareKey]);

    if (result[1] === 0) {
        var output = result[0].split("\n"); 
        var installationPath = null;
        var version = null;

        for (var i = 0; i < output.length; i++) {
            var line = output[i].trim();
            if (line.indexOf("InstallationPath") !== -1) {
                var parts = line.split("REG_SZ");
                if (parts.length > 1) {
                    installationPath = parts[1].trim();
                }
            } else if (line.indexOf("Version") !== -1) {
                var parts = line.split("REG_SZ");
                if (parts.length > 1) {
                    version = parts[1].trim();
                }
            }
        }

        if (installationPath && version) {
            return {
                path: cleanPath(installationPath),
                version: cleanPath(version)
            };
        }
    }

    return null;
}

function tempInstallerPath() {
    var registryKey = "HKCU\\Software\\BonV\\Temp";
    var registryQuery = installer.execute("cmd.exe", ["/c", "reg", "query", registryKey]);

    if (registryQuery[1] === 0) {
        var output = registryQuery[0].split("\n"); 
        
        for (var i = 0; i < output.length; i++) {
            var line = output[i].trim();
            if (line.indexOf("InstallerPath") !== -1) {
                var parts = line.split("REG_SZ");
                if (parts.length > 1) {
                    var path = parts[1].trim();
                    console.log("Found temp installer path: " + path);
                    return path;
                }
            }
        }
    }

    console.log("No temp installer path found");
    return null;
}

function addTempInstallerPath() {
    try {
        var currentInstallerPath = cleanPath(installer.value("InstallerFilePath"));

        if (!currentInstallerPath || currentInstallerPath === "") {
            console.log("InstallerFilePath was not available; trying process lookup");
            var wmicResult = installer.execute("cmd.exe", [
                "/c",
                "wmic process where \"name like '%BonVGroundStation%' or name like '%Installer%'\" get ExecutablePath /format:list"
            ]);

            if (wmicResult[1] === 0 && wmicResult[0]) {
                var lines = wmicResult[0].split("\n");
                for (var i = 0; i < lines.length; i++) {
                    var line = lines[i].trim();
                    if (line.indexOf("ExecutablePath=") === 0) {
                        currentInstallerPath = cleanPath(line.substring("ExecutablePath=".length));
                        if (currentInstallerPath) {
                            break;
                        }
                    }
                }
            }
        }
        
        if (!currentInstallerPath || currentInstallerPath === "") {
            console.log("ERROR: Unable to determine installer path");
            return false;
        }

        console.log("Saving installer path: " + currentInstallerPath);

        var registryKey = "HKCU\\Software\\BonV\\Temp";
        installer.execute("cmd.exe", ["/c", "reg add \"" + registryKey + "\" /f"]);
        
        var addCmd = "reg add \"" + registryKey + "\" /v InstallerPath /t REG_SZ /d \"" + 
                     currentInstallerPath + "\" /f";
        
        var result = installer.execute("cmd.exe", ["/c", addCmd]);
        
        if (result[1] === 0) {
            console.log("Successfully saved temp installer path");
            return true;
        } else {
            console.log("Failed to save temp installer path");
            return false;
        }
    } catch (e) {
        console.log("addTempInstallerPath error: " + e);
        return false;
    }
}

function removeTempInstallerPath() {
    try {
        var registryKey = "HKCU\\Software\\BonV\\Temp";
        var regDel = "reg delete \"" + registryKey + "\" /f";
        var result = installer.execute("cmd.exe", ["/c", regDel]);
        console.log("Removed temp installer path (exit code: " + result[1] + ")");
    } catch (e) {
        console.log("removeTempInstallerPath error: " + e);
    }
}

function cleanPath(str) {
    if (!str) return "";
    return str.trim().replace(/^"|"$/g, '');
}

function compareVersions(installedVersion, installerVersion) {
    var installedParts = installedVersion.split('.').map(Number);
    var installerParts = installerVersion.split('.').map(Number);
    var maxLength = Math.max(installedParts.length, installerParts.length);
    
    for (var i = 0; i < maxLength; i++) {
        var installedPart = installedParts[i] || 0;
        var installerPart = installerParts[i] || 0;

        if (installedPart > installerPart) return 1;
        if (installedPart < installerPart) return -1;
    }
    return 0;
}
