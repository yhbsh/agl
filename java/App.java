package com.example.java;

import android.app.*;
import android.content.*;
import android.database.*;
import android.graphics.*;
import android.location.*;
import android.media.*;
import android.net.*;
import android.os.*;
import android.preference.*;
import android.provider.*;
import android.telephony.*;
import android.text.*;
import android.util.*;
import android.view.*;
import android.widget.*;
import java.io.*;
import java.net.*;

public class App extends Activity {
    static final int CROSS_AXIS_COUNT = 3;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        GridView gridView = new GridView(this);
        gridView.setAdapter(new GridViewAdapter(this));
        gridView.setNumColumns(CROSS_AXIS_COUNT);
        gridView.setVerticalSpacing(8);
        gridView.setVerticalScrollBarEnabled(false);
        gridView.setGravity(Gravity.CENTER);
        gridView.setBackgroundColor(Color.BLACK);
        setContentView(gridView);
    }

    public class GridViewAdapter extends BaseAdapter {
        private Context context;

        public GridViewAdapter(Context context) {
            this.context = context;
        }

        @Override
        public int getCount() {
            return 50;
        }

        @Override
        public Object getItem(int position) {
            return null;
        }

        @Override
        public long getItemId(int position) {
            return position;
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            View view;
            if (convertView == null) {
                view = new View(context);
            } else {
                view = convertView;
            }

            int w = parent.getWidth() / CROSS_AXIS_COUNT - 16;
            int h = (int)(w * 1.6);
            view.setLayoutParams(new GridView.LayoutParams(w, h));
            view.setBackgroundColor(Color.RED);
            return view;
        }
    }
}
