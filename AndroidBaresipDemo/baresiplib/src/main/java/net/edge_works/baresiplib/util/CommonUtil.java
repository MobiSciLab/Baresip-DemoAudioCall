package net.edge_works.baresiplib.util;

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.support.annotation.Nullable;

/**
 * Created by thaile on 8/11/16.
 */
public class CommonUtil {

  public interface DialogListener{
    void onItemClicked(int position);
  }

  public static void showListSelectionDialog(Context context, String title, String[] values, @Nullable final DialogListener dialogListener){
    final AlertDialog.Builder builder = new AlertDialog.Builder(context);
    builder.setTitle(title).setCancelable(true)
        .setItems(values, new DialogInterface.OnClickListener() {
          public void onClick(DialogInterface dialog, int which) {
            // The 'which' argument contains the index position
            // of the selected item
            if(dialogListener != null){
              dialogListener.onItemClicked(which);
            }
            dialog.dismiss();
          }
        });
    builder.create().show();
  }
}
