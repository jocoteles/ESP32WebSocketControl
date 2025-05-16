// LittleFsManager.cpp
// This sketch provides an interactive way to manage the LittleFS filesystem on an ESP32.
// It allows formatting the partition if it's not already formatted,
// listing files, and option to re-format for a complete clear.

#include "FS.h"
#include "LittleFS.h"

void listDir(fs::FS &fs, const char *dirname, uint8_t levels);
bool directoryHasContents(fs::FS &fs, const char *dirname);

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(100); // Wait for Serial
  }
  delay(1000);

  Serial.println("\n--- LittleFS Interactive Management (Format for Clear) ---");

  // --- 1. Attempt to mount LittleFS without formatting ---
  Serial.println("\n[Phase 1] Attempting to mount LittleFS (without formatting)...");
  bool fsMounted = LittleFS.begin(false); // false = do NOT format on fail

  if (!fsMounted) {
    Serial.println("LittleFS mount failed. Partition might be unformatted or corrupted.");
    Serial.print("Do you want to format the LittleFS partition? (Y/N): ");
    String confirmation = "";
    while (Serial.available() == 0) { delay(100); }
    confirmation = Serial.readStringUntil('\n');
    confirmation.trim();

    if (confirmation.equalsIgnoreCase("Y")) {
      Serial.println("Formatting LittleFS... This will ERASE ALL DATA!");
      if (LittleFS.format()) {
        Serial.println("LittleFS partition formatted successfully.");
        if (LittleFS.begin(false)) {
          Serial.println("LittleFS mounted successfully after formatting (now empty).");
          fsMounted = true;
        } else {
          Serial.println("CRITICAL ERROR: Failed to mount LittleFS after formatting.");
        }
      } else {
        Serial.println("CRITICAL ERROR: LittleFS formatting failed.");
      }
    } else {
      Serial.println("Formatting cancelled. LittleFS remains unmounted/unformatted.");
    }
  }

  // --- 2. If LittleFS is mounted, list files and offer re-format to clear all ---
  if (fsMounted) {
    Serial.println("\n[Phase 2] LittleFS is mounted.");
    Serial.println("\nListing contents of the root directory ('/'):");
    listDir(LittleFS, "/", 1); // List root and one level of subdirs for better overview

    if (directoryHasContents(LittleFS, "/")) {
      Serial.println("\nFiles/directories found on the partition.");
      Serial.print("To delete ALL files AND directories, the partition will be RE-FORMATTED.");
      Serial.print("\nDo you want to re-format (clear everything)? (Y/N): ");
      String reformatConfirmation = "";
      while (Serial.available() == 0) { delay(100); }
      reformatConfirmation = Serial.readStringUntil('\n');
      reformatConfirmation.trim();

      if (reformatConfirmation.equalsIgnoreCase("Y")) {
        Serial.println("Re-formatting LittleFS to clear all contents... This will ERASE ALL DATA!");
        // Unmount first for a clean re-format, though format() might handle it.
        LittleFS.end(); 
        if (LittleFS.format()) {
          Serial.println("LittleFS partition re-formatted successfully.");
          // Mount again to leave it in a usable state
          if (LittleFS.begin(false)) {
            Serial.println("LittleFS mounted successfully after re-formatting (now empty).");
            Serial.println("\nListing directory again (should be empty):");
            listDir(LittleFS, "/", 0);
          } else {
            Serial.println("CRITICAL ERROR: Failed to mount LittleFS after re-formatting.");
          }
        } else {
          Serial.println("CRITICAL ERROR: LittleFS re-formatting failed.");
        }
      } else {
        Serial.println("Re-formatting (clear all) cancelled by user.");
      }
    } else {
      Serial.println("\nPartition is already empty.");
    }
  } else {
    Serial.println("\n[Phase 2] Skipped: LittleFS is not mounted.");
  }

  Serial.println("\n--- Management process complete ---");
}

void loop() {
  delay(10000);
}

bool directoryHasContents(fs::FS &fs, const char *dirname) {
    File root = fs.open(dirname);
    if (!root || !root.isDirectory()) {
        return false;
    }
    File file = root.openNextFile();
    bool hasContent = (file); // If openNextFile returns a valid File, there's content
    if (file) file.close();
    root.close();
    return hasContent;
}

void listDir(fs::FS &fs, const char *dirname, uint8_t levels) {
  Serial.printf("Listing directory: %s\n", dirname);
  File root = fs.open(dirname);
  if (!root) { Serial.println("- Failed to open directory"); return; }
  if (!root.isDirectory()) { Serial.println(" - Not a directory"); root.close(); return; }

  File file = root.openNextFile();
  bool foundSomething = false;
  while (file) {
    foundSomething = true;
    if (file.isDirectory()) {
      Serial.print("  DIR : "); Serial.println(file.name());
      if (levels > 0) {
        String path = String(dirname);
        if (path.length() > 0 && path.charAt(path.length() - 1) != '/') { path += "/"; }
        if (path == "/" && String(file.name()).startsWith("/")) {
             // Avoid "//" if file.name() from root already includes leading slash
             path = file.name();
        } else if (path == "/") {
             path += file.name();
        } else {
             path += file.name();
        }
        listDir(fs, path.c_str(), levels - 1);
      }
    } else {
      Serial.print("  FILE: "); Serial.print(file.name());
      Serial.print("\tSIZE: "); Serial.println(file.size());
    }
    file.close();
    file = root.openNextFile();
  }
  if (!foundSomething) { Serial.println("  (Directory is empty or no more entries)"); }
  root.close();
}