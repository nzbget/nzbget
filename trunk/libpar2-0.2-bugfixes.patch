diff -aud -U 5 ../libpar2-0.2-original/par2repairer.cpp ../libpar2-0.2/par2repairer.cpp
--- ../libpar2-0.2-original/par2repairer.cpp	2006-01-20 18:25:20.000000000 +0100
+++ ../libpar2-0.2/par2repairer.cpp	2012-11-30 14:23:31.000000000 +0100
@@ -76,10 +76,11 @@
     ++sf;
   }
 
   delete mainpacket;
   delete creatorpacket;
+  delete headers;
 }
 
 
 Result Par2Repairer::PreProcess(const CommandLine &commandline)
 {
@@ -1259,11 +1260,11 @@
         string path;
         string name;
         DiskFile::SplitFilename(filename, path, name);
 
         cout << "Target: \"" << name << "\" - missing." << endl;
-	sig_done.emit(name, 0, sourcefile->GetVerificationPacket()->BlockCount());
+	sig_done.emit(name, 0, sourcefile->GetVerificationPacket() ? sourcefile->GetVerificationPacket()->BlockCount() : 0);
       }
     }
 
     ++sf;
   }
@@ -1802,11 +1803,11 @@
              << "\" - no data found." 
              << endl;
       }
     }
   }
-  sig_done.emit(name,count,sourcefile->GetVerificationPacket()->BlockCount()); 
+  sig_done.emit(name,count, count>0 && sourcefile->GetVerificationPacket() ? sourcefile->GetVerificationPacket()->BlockCount() : 0); 
   sig_progress.emit(1000.0);
   return true;
 }
 
 // Find out how much data we have found
diff -aud -U 5 ../libpar2-0.2-original/par2repairer.h ../libpar2-0.2/par2repairer.h
--- ../libpar2-0.2-original/par2repairer.h	2006-01-20 00:38:27.000000000 +0100
+++ ../libpar2-0.2/par2repairer.h	2012-11-30 14:24:46.000000000 +0100
@@ -34,10 +34,15 @@
   sigc::signal<void, std::string> sig_filename;
   sigc::signal<void, double> sig_progress;
   sigc::signal<void, ParHeaders*> sig_headers;
   sigc::signal<void, std::string, int, int> sig_done;
 
+  // This method allows to determine whether libpar2 includes the patches
+  // ("libpar2-0.2-bugfixes.patch") submitted to libpar2 project.
+  // Use the method in configure scripts for detection.
+  void BugfixesPatchVersion2() { }
+
 protected:
   // Steps in verifying and repairing files:
 
   // Load packets from the specified file
   bool LoadPacketsFromFile(string filename);
