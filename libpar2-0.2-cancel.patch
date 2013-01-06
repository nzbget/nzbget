diff -aud -U 5 ../libpar2-0.2-original/par2repairer.cpp ../libpar2-0.2/par2repairer.cpp
--- ../libpar2-0.2-original/par2repairer.cpp	2012-12-03 10:47:04.000000000 +0100
+++ ../libpar2-0.2/par2repairer.cpp	2012-12-03 10:48:13.000000000 +0100
@@ -50,10 +50,12 @@
   outputbuffer = 0;
 
   noiselevel = CommandLine::nlNormal;
   headers = new ParHeaders;
   alreadyloaded = false;
+
+  cancelled = false;
 }
 
 Par2Repairer::~Par2Repairer(void)
 {
   delete [] (u8*)inputbuffer;
@@ -404,10 +406,14 @@
         {
           cout << "Loading: " << newfraction/10 << '.' << newfraction%10 << "%\r" << flush;
           progress = offset;
 	sig_progress.emit(newfraction);
 
+          if (cancelled)
+          {
+            break;
+          }
         }
       }
 
       // Attempt to read the next packet header
       PACKET_HEADER header;
@@ -582,10 +588,15 @@
     if (noiselevel > CommandLine::nlQuiet)
       cout << "No new packets found" << endl;
     delete diskfile;
   }
   
+  if (cancelled)
+  {
+    return false;
+  }
+
   return true;
 }
 
 // Finish loading a recovery packet
 bool Par2Repairer::LoadRecoveryPacket(DiskFile *diskfile, u64 offset, PACKET_HEADER &header)
@@ -831,26 +842,42 @@
 
     // Load packets from each file that was found
     for (list<string>::const_iterator s=files->begin(); s!=files->end(); ++s)
     {
       LoadPacketsFromFile(*s);
+      if (cancelled)
+      {
+        break;
+      }
     }
 
     delete files;
+    if (cancelled)
+    {
+      return false;
+    }
   }
 
   {
     string wildcard = name.empty() ? "*.PAR2" : name + ".*.PAR2";
     list<string> *files = DiskFile::FindFiles(path, wildcard);
 
     // Load packets from each file that was found
     for (list<string>::const_iterator s=files->begin(); s!=files->end(); ++s)
     {
       LoadPacketsFromFile(*s);
+      if (cancelled)
+      {
+        break;
+      }
     }
 
     delete files;
+    if (cancelled)
+    {
+      return false;
+    }
   }
 
   return true;
 }
 
@@ -864,13 +891,22 @@
     // If the filename contains ".par2" anywhere
     if (string::npos != filename.find(".par2") ||
         string::npos != filename.find(".PAR2"))
     {
       LoadPacketsFromFile(filename);
+      if (cancelled)
+      {
+        break;
+      }
     }
   }
 
+  if (cancelled)
+  {
+    return false;
+  }
+
   return true;
 }
 
 // Check that the packets are consistent and discard any that are not
 bool Par2Repairer::CheckPacketConsistency(void)
@@ -1208,10 +1244,15 @@
 
   // Start verifying the files
   sf = sortedfiles.begin();
   while (sf != sortedfiles.end())
   {
+    if (cancelled)
+    {
+      return false;
+    }
+
     // Do we have a source file
     Par2RepairerSourceFile *sourcefile = *sf;
 
     // What filename does the file use
     string filename = sourcefile->TargetFileName();
@@ -1560,10 +1601,14 @@
       if (oldfraction != newfraction)
       {
         cout << "Scanning: \"" << shortname << "\": " << newfraction/10 << '.' << newfraction%10 << "%\r" << flush;
 	sig_progress.emit(newfraction);
 
+        if (cancelled)
+        {
+          break;
+        }
       }
     }
 
     // If we fail to find a match, it might be because it was a duplicate of a block
     // that we have already found.
@@ -1649,10 +1694,15 @@
           return false;
       }
     }
   }
 
+  if (cancelled)
+  {
+    return false;
+  }
+
   // Get the Full and 16k hash values of the file
   filechecksummer.GetFileHashes(hashfull, hash16k);
 
   // Did we make any matches at all
   if (count > 0)
@@ -2289,14 +2339,23 @@
           if (oldfraction != newfraction)
           {
             cout << "Repairing: " << newfraction/10 << '.' << newfraction%10 << "%\r" << flush;
 	    sig_progress.emit(newfraction);
 
+            if (cancelled)
+            {
+              break;
+            }
           }
         }
       }
 
+      if (cancelled)
+      {
+        break;
+      }
+
       ++inputblock;
       ++inputindex;
     }
   }
   else
@@ -2346,13 +2405,22 @@
         if (oldfraction != newfraction)
         {
           cout << "Processing: " << newfraction/10 << '.' << newfraction%10 << "%\r" << flush;
 	sig_progress.emit(newfraction);
 
+          if (cancelled)
+          {
+            break;
+          }
         }
       }
 
+      if (cancelled)
+      {
+        break;
+      }
+
       ++copyblock;
       ++inputblock;
     }
   }
 
@@ -2360,10 +2428,15 @@
   if (lastopenfile != NULL)
   {
     lastopenfile->Close();
   }
 
+  if (cancelled)
+  {
+    return false;
+  }
+
   if (noiselevel > CommandLine::nlQuiet)
     cout << "Writing recovered data\r";
 
   // For each output block that has been recomputed
   vector<DataBlock*>::iterator outputblock = outputblocks.begin();
diff -aud -U 5 ../libpar2-0.2-with-bugfixes-patch/par2repairer.h ../libpar2-0.2/par2repairer.h
--- ../libpar2-0.2-original/par2repairer.h	2012-12-03 10:47:04.000000000 +0100
+++ ../libpar2-0.2/par2repairer.h	2012-12-03 10:48:13.000000000 +0100
@@ -186,8 +186,9 @@
 
   u64                       progress;                // How much data has been processed.
   u64                       totaldata;               // Total amount of data to be processed.
   u64                       totalsize;               // Total data size
 
+  bool                      cancelled;               // repair cancelled
 };
 
 #endif // __PAR2REPAIRER_H__
